/* I1086b: the Search hits are an engine model: after a library write (a sync finishing) the engine runs the shown query again by itself
 * (lib_models_follow), with no new SOURCE_SEARCH sent. Through the real entry point (messages in, pump): VIEW_SOURCE search, one
 * SOURCE_SEARCH, then a LIBRARY_SYNC. PASS: the search source is called a second time (sentinel: g_search_calls == 2) although only one
 * SOURCE_SEARCH line was sent. With the search re-run removed from lib_models_follow it stays 1 and the test fails. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/source.h>

#include "db/db.h"
#include "source.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static atomic_int g_done;
static atomic_uint g_search_calls;
static sel_library* g_lib;

static void nap(long ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = ms * 1000000L;
    nanosleep(&ts, NULL);
}

static int quick_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    (void)host; (void)arg; (void)now; (void)cap;
    stats[0].tag = SEL_TAG_END;
    stats[0].value = 0;
    atomic_store(&g_done, 1);
    return 1;
}

static int quick_search(const sel_host* host, const char* q, sel_search_sink* out) {
    (void)host;
    atomic_fetch_add(&g_search_calls, 1u);
    nap(30);
    out->row(out->ctx, q, "Title", "Artist", "mod");
    return 1;
}

static const sel_source g_sync_src = {.id = "i86sync", .name = "I1086 sync", .home = "", .about = "Syncs at once.", .sync = quick_sync, .flags = SEL_SOURCE_SYNCS};
static const sel_source g_search_src = {.id = "i86srch", .name = "I1086 search", .home = "", .about = "Answers a search.", .search = quick_search};

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static int send_line(sel_engine* e, const char* line) {
    static char pool[4][256];
    static unsigned next;

    return sel_engine_send_line(e, line, pool[next++ & 3u], sizeof pool[0]) == 0;
}

#define CHECK(c)                                                \
    do {                                                        \
        if (!(c)) {                                             \
            printf("FAIL line %d: %s\n", __LINE__, #c);         \
            return 1;                                           \
        }                                                       \
    } while (0)

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1086b-XXXXXX";
    char db[128];
    sel_engine_config cfg;
    sel_engine* e;
    unsigned i;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    g_lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(g_lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = g_lib;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    sel_source_register_builtin();
    CHECK(sel_source_register(&g_sync_src));
    CHECK(sel_source_register(&g_search_src));

    CHECK(send_line(e, "VIEW_SOURCE search"));
    sel_engine_pump(e, sink, NULL);
    CHECK(send_line(e, "SOURCE_SEARCH i86srch \"one\""));
    for (i = 0; i < 500 && atomic_load(&g_search_calls) < 1u; i++) {
        sel_engine_pump(e, sink, NULL);
        nap(2);
    }
    CHECK(atomic_load(&g_search_calls) == 1u);
    CHECK(send_line(e, "LIBRARY_SYNC i86sync /a 1")); /* the only message after the search: a library write */
    for (i = 0; i < 1000 && (!atomic_load(&g_done) || atomic_load(&g_search_calls) < 2u); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(2);
    }
    CHECK(atomic_load(&g_done) == 1);
    CHECK(atomic_load(&g_search_calls) == 2u); /* I1086b: the engine ran the query again, no SOURCE_SEARCH came from a client */

    sel_engine_destroy(e);
    sel_library_close(g_lib);
    unlink(path);
    unlink(db);
    puts("search_follows_library_ok");
    return 0;
}
