/* I1044: a search must not cancel a sync. Searches run on their own threads and hold no library task slot, so the slot may
 * hold a scan or a sync; the old search_supersede asked the library task to cancel whenever a search was running, which
 * cancelled that sync as soon as a second query replaced the first. Through the real entry point (messages in, pump) a sync
 * is held open, two searches are sent, and the sync is released afterwards. PASS: the sync never saw a cancel request while
 * it ran (sentinel: g_cancel_seen) and ended normally (g_done); both searches ran (g_search_calls == 2). With the cancel
 * request back in search_supersede, g_cancel_seen is set and the test fails. */
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

static atomic_int g_in_sync, g_release, g_done, g_cancel_seen;
static atomic_uint g_search_calls;
static sel_library* g_lib;

static void nap(long ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = ms * 1000000L;
    nanosleep(&ts, NULL);
}

static int held_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    int waited = 0;

    (void)host;
    (void)arg;
    (void)now;
    (void)cap;
    atomic_store(&g_in_sync, 1);
    while (!atomic_load(&g_release) && waited < 5000) {
        if (sel_library_task_cancelled(g_lib)) {
            atomic_store(&g_cancel_seen, 1);
        }
        nap(1);
        waited++;
    }
    if (sel_library_task_cancelled(g_lib)) {
        atomic_store(&g_cancel_seen, 1);
    }
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

static const sel_source g_sync_src = {
    "i1044sync", "I1044 sync", "", "Held open in sync.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, held_sync, SEL_SOURCE_SYNCS, NULL, NULL,
    NULL, NULL,
};
static const sel_source g_search_src = {
    "i1044srch", "I1044 search", "", "Answers a search.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL,
    quick_search, NULL,
};

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
    char path[] = "/tmp/sel-i1044-XXXXXX";
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
    CHECK(sel_source_register(&g_sync_src));
    CHECK(sel_source_register(&g_search_src));

    CHECK(send_line(e, "LIBRARY_SYNC i1044sync /a 1"));
    sel_engine_pump(e, sink, NULL);
    for (i = 0; i < 2000 && !atomic_load(&g_in_sync); i++) {
        nap(1);
    }
    CHECK(atomic_load(&g_in_sync) == 1); /* the sync holds the library task slot */
    CHECK(send_line(e, "SOURCE_SEARCH i1044srch \"one\""));
    sel_engine_pump(e, sink, NULL);
    CHECK(send_line(e, "SOURCE_SEARCH i1044srch \"two\"")); /* supersedes the first while its thread may still run */
    for (i = 0; i < 300; i++) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }
    CHECK(atomic_load(&g_search_calls) == 2u);
    CHECK(atomic_load(&g_done) == 0); /* still held: the searches did not end it */
    atomic_store(&g_release, 1);
    for (i = 0; i < 5000 && sel_engine_scan_busy(e); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }
    CHECK(atomic_load(&g_done) == 1);
    CHECK(atomic_load(&g_cancel_seen) == 0); /* I1044: no cancel request reached the sync */

    sel_engine_destroy(e);
    sel_library_close(g_lib);
    unlink(path);
    unlink(db);
    puts("search_keeps_sync_ok");
    return 0;
}
