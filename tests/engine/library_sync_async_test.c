/* I527: LIBRARY_SYNC runs on the library job thread, so the shell loop (the pump) never waits for a network sync.
 * The desktop UI froze (beachball) while a sync sat in sel_net_recv on the shell thread. A registered source whose sync
 * blocks until the test releases it stands in for the network. Through the real entry point (message in, pump) the test
 * proves: the pump returns many times while the sync is still running (sentinel: g_in_sync set, g_done clear), a second
 * LIBRARY_SYNC and a LIBRARY_SCAN that arrive meanwhile queue instead of failing, no LIBRARY_RESULT arrives before the
 * release, and every queued sync has run after it, also when more than 4 queue (one per source, as the sync-all
 * banner sends them). With the old inline cmd_LIBRARY_SYNC the first pump does not return until
 * the sync ended (its own 3 s safety timeout), so g_in_sync/g_done are already set and the test fails. */
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

static atomic_int g_in_sync, g_release, g_done;
static atomic_uint g_calls;

static void nap(long ms) {
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = ms * 1000000L;
    nanosleep(&ts, NULL);
}

static int blocking_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    int waited = 0;

    (void)host;
    (void)arg;
    (void)now;
    (void)cap;
    atomic_fetch_add(&g_calls, 1u);
    atomic_store(&g_in_sync, 1);
    while (!atomic_load(&g_release) && waited < 3000) { /* the stand-in for sel_net_recv; bounded so a regression cannot hang */
        nap(1);
        waited++;
    }
    stats[0].tag = SEL_TAG_END;
    stats[0].value = 0;
    atomic_store(&g_done, 1);
    return 1;
}

static const sel_source g_src = {
    "i527block", "I527 blocking", "", "Blocks in sync until released.", NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, blocking_sync, SEL_SOURCE_SYNCS, NULL,
};

static unsigned g_results, g_errors, g_busy;

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_LIBRARY_RESULT) {
        g_results++;
    } else if (evt->id == SEL_EVT_ERROR) {
        g_errors++;
        g_busy += evt->argc > 0 && strcmp(evt->args[0].v.s, "sync_busy") == 0;
    }
}

static int send_line(sel_engine* e, const char* line) {
    static char pool[4][256]; /* the decoded strings stay in their pool until the engine pumps; one pool per line */
    static unsigned next;

    return sel_engine_send_line(e, line, pool[next++ & 3u], sizeof pool[0]) == 0;
}

#define CHECK(c)                                                      \
    do {                                                              \
        if (!(c)) {                                                   \
            printf("FAIL line %d: %s\n", __LINE__, #c);               \
            return 1;                                                 \
        }                                                             \
    } while (0)

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i527-XXXXXX";
    char db[128];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    unsigned pumps_while_running = 0, i;
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    CHECK(sel_source_register(&g_src));

    CHECK(send_line(e, "LIBRARY_SYNC i527block /a 1"));
    sel_engine_pump(e, sink, NULL); /* with the old inline sync this does not return until the sync ended */
    CHECK(atomic_load(&g_done) == 0);
    for (i = 0; i < 2000 && !atomic_load(&g_in_sync); i++) {
        nap(1);
    }
    CHECK(atomic_load(&g_in_sync) == 1); /* the sync is running on the job thread, blocked */
    CHECK(sel_engine_scan_busy(e) == 1);
    CHECK(send_line(e, "LIBRARY_SYNC i527block /b 2")); /* queues behind the running sync */
    CHECK(send_line(e, "LIBRARY_SCAN /nonexistent-i527")); /* shares the task slot: queues, does not fail or stomp */
    sel_engine_pump(e, sink, NULL);
    for (i = 0; i < 5; i++) { /* the sync-all banner sends one per due source: more than 4 must all queue, none sync_busy */
        char line[64];

        snprintf(line, sizeof line, "LIBRARY_SYNC i527block /c%u 3", i);
        CHECK(send_line(e, line));
        sel_engine_pump(e, sink, NULL);
    }
    for (i = 0; i < 50; i++) {
        sel_engine_pump(e, sink, NULL); /* each pump returns while the sync still blocks */
        pumps_while_running += atomic_load(&g_done) == 0;
        nap(1);
    }
    CHECK(pumps_while_running == 50);
    CHECK(g_results == 0 && g_errors == 0);
    CHECK(atomic_load(&g_calls) == 1u);

    atomic_store(&g_release, 1);
    for (i = 0; i < 5000 && (atomic_load(&g_calls) < 7u || sel_engine_scan_busy(e)); i++) {
        sel_engine_pump(e, sink, NULL);
        nap(1);
    }
    CHECK(atomic_load(&g_calls) == 7u); /* every queued sync ran after the first */
    CHECK(g_busy == 0); /* no sync dropped (the scan of a missing path may report its own error) */
    CHECK(g_results >= 7);

    sel_engine_destroy(e);
    sel_library_close(lib);
    unlink(path);
    unlink(db);
    puts("library_sync_async_ok");
    return 0;
}
