/* I1091 (I1038): re-running the shown query must not publish an empty intermediate state. A library change makes lib_models_follow post the
 * folders read next to the Search re-run. The test db has no folders, so the folders answer is an empty tree; written into the shared
 * row store (Home, Folders and Search share it) it would replace the shown Search rows. Order made deterministic: the re-run search
 * holds (g_hold) until the folders read thread is done, the pump does not run meanwhile, so one pump collects the search buckets and
 * then the folders answer. PASS: at every pump the Search view holds rows (> 0), and sentinel stats.hidden_read_drops >= 1 proves the
 * folders answer was collected while Search was shown. With the two source gates in lib_reads_collect removed it fails. */
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
static atomic_int g_hold;
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
    while (atomic_load(&g_hold)) nap(2);
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

static uint32_t models(sel_engine* e) {
    sel_engine_stats s;

    sel_engine_stats_get(e, &s);
    return s.lib_models_followed;
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
    sel_engine_stats st;
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
    CHECK(sel_engine_state(e)->view->source == SEL_VIEW_SEARCH);
    CHECK(sel_engine_state(e)->view->row_count > 0u);
    atomic_store(&g_hold, 1); /* the re-run search answers only when released */
    CHECK(send_line(e, "LIBRARY_SYNC i86sync /a 1"));
    for (i = 0; i < 1000 && (!atomic_load(&g_done) || models(e) < 1u); i++) {
        sel_engine_pump(e, sink, NULL);
        CHECK(sel_engine_state(e)->view->row_count > 0u);
        nap(2);
    }
    CHECK(atomic_load(&g_done) == 1);
    CHECK(models(e) >= 1u);
    nap(150); /* no pump: the folders read thread finishes first */
    atomic_store(&g_hold, 0);
    nap(250); /* no pump: the held search answers too */
    for (i = 0; i < 100; i++) {
        sel_engine_pump(e, sink, NULL);
        CHECK(sel_engine_state(e)->view->source == SEL_VIEW_SEARCH);
        CHECK(sel_engine_state(e)->view->row_count > 0u); /* I1091: never an empty Search page */
        nap(2);
    }
    CHECK(atomic_load(&g_search_calls) >= 2u); /* I1086b kept: the engine ran the query again */
    sel_engine_stats_get(e, &st);
    CHECK(st.hidden_read_drops >= 1u); /* sentinel: a folders answer was collected while Search was shown */

    sel_engine_destroy(e);
    sel_library_close(g_lib);
    unlink(path);
    unlink(db);
    puts("search_publish_never_empty_ok");
    return 0;
}
