/* I1231: the fan-out SOURCE_SEARCH * "<q>" answers every synced catalogue source from ONE grouped library request (an FTS query
 * over song_fts, grouped by source) instead of one LIKE scan per source behind the library call lock.
 * Three stand-in sources carry SEL_SOURCE_SYNCS | SEL_SOURCE_CATALOGUE_SEARCH (two with synced rows, one never synced) plus
 * one plain searchable source without the flag. Through the real entry point (message in, pump out) the test proves:
 *  - both synced sections get their hits and search_done counts (g1=2, g2=1) from the grouped request;
 *  - sentinel: sel_library_search_counts says the grouped request ran exactly once and the per-source LIKE search never ran,
 *    and the engine stats count one catalogue group;
 *  - the synced sources' own search hooks never ran (they would add a "/hook" row); the never-synced one fell back to its
 *    hook (live search, SEL_CATALOGUE_NOT_SYNCED), and the unflagged source ran its hook as before.
 * With search_start_group removed every flagged source runs its hook (one request each) and the test fails. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "engine_test_db_lock.h"
#include "source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char g_hooks[256];
static char g_done[256];
static unsigned g_rows[4];
static unsigned g_hook_rows;
static int g_fail;

static void nap(long ms) {
    struct timespec ts = {0, ms * 1000000L};
    nanosleep(&ts, NULL);
}

static int hook(const char* id, const sel_host* host, const char* q, sel_search_sink* out) {
    size_t n = strlen(g_hooks);
    (void)host;
    snprintf(g_hooks + n, sizeof g_hooks - n, "%s:%s;", id, q);
    return out->row(out->ctx, "x/hook", "Hook", "Live", "MOD") ? 1 : 0;
}
static int hook_g1(const sel_host* h, const char* q, sel_search_sink* o) { return hook("i1231g1", h, q, o); }
static int hook_g2(const sel_host* h, const char* q, sel_search_sink* o) { return hook("i1231g2", h, q, o); }
static int hook_g3(const sel_host* h, const char* q, sel_search_sink* o) { return hook("i1231g3", h, q, o); }
static int hook_p(const sel_host* h, const char* q, sel_search_sink* o) { return hook("i1231p", h, q, o); }

#define SRC(ID, FLAGS, HOOK) { ID, ID, "", "I1231 stand-in.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, FLAGS, NULL, NULL, HOOK, NULL }
#define CAT (SEL_SOURCE_SYNCS | SEL_SOURCE_CATALOGUE_SEARCH)
static const sel_source g_g1 = SRC("i1231g1", CAT, hook_g1), g_g2 = SRC("i1231g2", CAT, hook_g2), g_g3 = SRC("i1231g3", CAT, hook_g3),
                       g_p = SRC("i1231p", 0u, hook_p);

static void sink(void* ctx, const sel_message* evt) {
    static const char* const ids[4] = {"i1231g1", "i1231g2", "i1231g3", "i1231p"};
    (void)ctx;
    if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        const sel_tag* t = evt->args[0].v.t;
        const char* kind = (const char*)sel_tag_find(t, SEL_SRC_KIND, (uintptr_t)"");
        const char* key = (const char*)sel_tag_find(t, SEL_SRC_KEY, (uintptr_t)"");
        const char* path = (const char*)sel_tag_find(t, SEL_SRC_PATH, (uintptr_t)"");
        unsigned i;

        if (strcmp(kind, "search") == 0) {
            for (i = 0; i < 4u; i++) {
                g_rows[i] += strcmp(key, ids[i]) == 0;
            }
            g_hook_rows += strstr(path, "x/hook") != NULL && (strcmp(key, "i1231g1") == 0 || strcmp(key, "i1231g2") == 0);
        } else if (strcmp(kind, "search_done") == 0) {
            size_t n = strlen(g_done);
            snprintf(g_done + n, sizeof g_done - n, "%s=%u;", key, (unsigned)sel_tag_find(t, SEL_SRC_COUNT, 0));
        }
    }
}

static int send_line(sel_engine* e, const char* line) {
    static char pool[4][2048];
    static unsigned next;
    return sel_engine_send_line(e, line, pool[next++ & 3u], sizeof pool[0]) == 0;
}

#define CHECK(c)                                                                          \
    do {                                                                                  \
        if (!(c)) {                                                                       \
            printf("FAIL line %d: %s\n  hooks=%s\n  done=%s\n", __LINE__, #c, g_hooks, g_done); \
            g_fail++;                                                                     \
        }                                                                                 \
    } while (0)

static void sql(sel_library* lib, const char* text) {
    char* err = NULL;
    int rc;
    db_enter(lib);
    rc = sqlite3_exec(lib->db, text, NULL, NULL, &err);
    sel_library_source_mirror_refresh(lib); /* the per-source song counts the grouped search checks (NOT_SYNCED) */
    db_leave(lib);
    if (rc != SQLITE_OK) {
        printf("FAIL sql: %s\n", err != NULL ? err : "?");
        g_fail++;
    }
}

static int has(const char* log, const char* tok) {
    return strstr(log, tok) != NULL;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1231-XXXXXX";
    char db[128];
    sel_engine_config cfg;
    sel_engine_stats st;
    sel_engine* e;
    sel_library* lib;
    uint32_t searches = 0, grouped = 0;
    unsigned i;
    int fd = mkstemp(path);

    if (fd < 0) {
        puts("FAIL mkstemp");
        return 1;
    }
    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    if (lib == NULL) {
        puts("FAIL open");
        return 1;
    }
    sql(lib, "INSERT INTO songs(key, source, path, name, artist, added) VALUES"
             "('i1231g1:a/jammer.mod','i1231g1','a/jammer.mod','Jammer','Ann',1),"
             "('i1231g1:a/jam2.mod','i1231g1','a/jam2.mod','Jam Session','Bob',1),"
             "('i1231g1:a/other.mod','i1231g1','a/other.mod','Other','Cy',1),"
             "('i1231g2:b/jamming.sid','i1231g2','b/jamming.sid','Jamming','Dee',1),"
             "('i1231g2:b/quiet.sid','i1231g2','b/quiet.sid','Quiet','Eve',1);");
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    e = sel_engine_create(host, &cfg);
    if (e == NULL) {
        puts("FAIL engine");
        return 1;
    }
    sel_source_register_builtin();
    if (!(sel_source_register(&g_g1) && sel_source_register(&g_g2) && sel_source_register(&g_g3) && sel_source_register(&g_p))) {
        puts("FAIL register");
        return 1;
    }
    /* only the stand-ins answer: every built-in searchable source is switched off, so no network is touched */
    {
        char line[1024] = "PREF_SET library.sources_off \"";
        uint32_t n = sel_source_count(), k;
        for (k = 0; k < n; k++) {
            const sel_source* s = sel_source_get(k);
            if (s != NULL && strncmp(s->id, "i1231", 5) != 0) {
                strncat(line, s->id, sizeof line - strlen(line) - 3u);
                strncat(line, ",", sizeof line - strlen(line) - 3u);
            }
        }
        strncat(line, "\"", sizeof line - strlen(line) - 1u);
        CHECK(send_line(e, line));
    }
    sel_engine_pump(e, sink, NULL);
    sel_library_search_counts(lib, &searches, &grouped);
    CHECK(grouped == 0u);

    CHECK(send_line(e, "SOURCE_SEARCH * \"jam\""));
    sel_engine_pump(e, sink, NULL);
    for (i = 0; i < 5000 && sel_engine_scan_busy(e); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }
    sel_engine_pump(e, sink, NULL);

    CHECK(g_rows[0] == 2u && g_rows[1] == 1u);       /* both synced sections filled from the synced rows */
    CHECK(has(g_done, "i1231g1=2;") && has(g_done, "i1231g2=1;"));
    CHECK(g_hook_rows == 0u);                        /* their live hooks never answered */
    CHECK(!has(g_hooks, "i1231g1:") && !has(g_hooks, "i1231g2:"));
    CHECK(has(g_hooks, "i1231g3:jam;"));             /* never synced: the live search still runs */
    CHECK(has(g_hooks, "i1231p:jam;") && g_rows[3] == 1u); /* no catalogue flag: unchanged */
    sel_library_search_counts(lib, &searches, &grouped);
    printf("grouped_requests=%u like_searches=%u\n", (unsigned)grouped, (unsigned)searches);
    CHECK(grouped == 1u);  /* sentinel: ONE library request filled every synced catalogue bucket */
    CHECK(searches == 0u); /* and no per-source LIKE search ran */
    sel_engine_stats_get(e, &st);
    CHECK(st.search_catalogue_groups == 1u);

    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    if (g_fail != 0) {
        return 1;
    }
    puts("source_search_grouped_ok");
    return 0;
}
