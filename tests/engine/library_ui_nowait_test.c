/* I829: the command thread never waits on the library worker. The desktop search field sends LIBRARY_SEARCH, VIEW_SOURCE search
 * and VIEW_WINDOW per keystroke; the Home chips send VIEW_SOURCE home:<chip>; the Sources page sends SOURCES and VIEW_SOURCE
 * folders. On a 614 000-song library the worker spends seconds in a home read or a scan batch commit, and a sync job holds the
 * library call lock between its requests, so a synchronous library request from the pump froze the Mac UI (beach ball).
 *
 * Fixture: a thread owns the library call lock, like a running sync or scan job, for HOLD_MS. Through the real entry point
 * (sel_engine_send_line + sel_engine_pump) the test sends the search-typing and source-view commands and asserts
 *   - the pump returns within PUMP_BUDGET_MS while the library is busy,
 *   - sel_library_watched_waits() counts 0 synchronous library requests from the pump thread (the sentinel),
 *   - once the library is free the answers arrive as messages: one LIBRARY_RESULT per search, the home and folder reads applied. */
#include <pthread.h>
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/registry.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"

enum { HOLD_MS = 1500, PUMP_BUDGET_MS = 50 };

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static volatile int g_held;
static uint32_t g_results;

static long ms_since(const struct timespec* t0) {
    struct timespec t1;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (long)((t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_nsec - t0->tv_nsec) / 1000000L);
}

/* Plays the sync job: owns the library call lock for HOLD_MS, then lets go (so the old, blocking code ends instead of hanging). */
static void* holder(void* arg) {
    sel_library* lib = arg;

    pthread_mutex_lock(&lib->call_lock);
    g_held = 1;
    usleep(HOLD_MS * 1000);
    pthread_mutex_unlock(&lib->call_lock);
    return NULL;
}

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->kind == SEL_MSG_EVT && evt->id == SEL_EVT_LIBRARY_RESULT) g_results++;
}

int main(void) {
    static const char* const lines[] = {"VIEW_SOURCE home",       "VIEW_SOURCE home:chiptunes", "LIBRARY_SEARCH \"c\"",
                                        "VIEW_SOURCE search",     "VIEW_WINDOW 40",             "LIBRARY_SEARCH \"ch\"",
                                        "VIEW_SOURCE search",     "VIEW_WINDOW 40",             "SOURCES",
                                        "VIEW_SOURCE folders"};
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_uinowait_XXXXXX";
    char db[256], pool[1024];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    pthread_t th;
    struct timespec t0;
    sel_engine_stats s0, s1;
    long pump_ms, worst = 0;
    uint32_t i, waits0, waits, searches0;
    int was;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    CHECK(sqlite3_exec(lib->db,
                       "INSERT INTO songs(key, source, path, name, artist, album, added) VALUES"
                       "('local:a.mod','local','a.mod','Chip one','Ann','Alb1',100),('local:c.sid','local','c.sid','Chime','Bob',NULL,300);",
                       NULL, NULL, NULL) == SQLITE_OK);
    sel_engine_stats_get(e, &s0);
    searches0 = s0.library_searches;

    pthread_create(&th, NULL, holder, lib);
    while (!g_held) usleep(1000);
    was = sel_library_watch(1); /* this thread is the pump thread: count every library request it makes */
    waits0 = sel_library_watched_waits();
    for (i = 0; i < sizeof lines / sizeof lines[0]; i++) {
        CHECK(sel_engine_send_line(e, lines[i], pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        if (pump_ms > worst) worst = pump_ms;
        if (pump_ms >= PUMP_BUDGET_MS) printf("i829 slow pump: '%s' took %ld ms\n", lines[i], pump_ms);
    }
    waits = sel_library_watched_waits() - waits0;
    printf("i829 busy library: worst pump %ld ms (budget %d), synchronous library waits on the pump thread %u\n", worst,
           PUMP_BUDGET_MS, (unsigned)waits);
    CHECK(worst < PUMP_BUDGET_MS);
    CHECK(waits == 0u);
    pthread_join(th, NULL);

    /* The library is free: the posted reads finish and their answers arrive through the pump. */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        sel_engine_pump(e, sink, NULL);
        sel_engine_stats_get(e, &s1);
        if (s1.library_searches >= searches0 + 1u && g_results >= 1u && s1.lib_reads_applied >= 3u) break;
        if (ms_since(&t0) > 5000) break;
        usleep(2000);
    }
    sel_engine_stats_get(e, &s1);
    printf("i829 after release: searches %u, LIBRARY_RESULT events %u, async reads applied %u, home rows %u\n",
           (unsigned)(s1.library_searches - searches0), (unsigned)g_results, (unsigned)s1.lib_reads_applied,
           (unsigned)s1.home_rows);
    CHECK(s1.library_searches >= searches0 + 1u);
    CHECK(g_results >= 1u);
    CHECK(s1.lib_reads_applied >= 3u); /* home, search and folders, each through the posted read */
    /* I961: once the library is free the pump's nowait reads (sources sync times via SEL_LIB_REQ_META, genre via SEL_LIB_REQ_GENRE)
     * get the call lock and are real round trips. They now count; the busy-library check above is the I829 proof. */
    printf("i961 nowait round trips on the pump thread after release: %u\n", (unsigned)(sel_library_watched_waits() - waits0));

    /* I961: with the call lock FREE a nowait request still makes a worker round trip. It must show in the watched stats. */
    {
        sel_message reply;
        uint32_t w1 = sel_library_watched_waits(), d1 = sel_library_deferred();
        int nw = sel_library_nowait(1), ok = 0, tries;
        /* A posted read may still hold the call lock: a deferred try is refused and not counted, so retry until one runs. */
        for (tries = 0; tries < 2000 && !ok; tries++) {
            d1 = sel_library_deferred();
            w1 = sel_library_watched_waits();
            ok = sel_library_call(lib, SEL_LIB_REQ_STATS, &reply);
            if (!ok) {
                CHECK(sel_library_watched_waits() == w1); /* deferred: no wait, not counted */
                usleep(1000);
            }
        }
        sel_library_nowait(nw);
        CHECK(ok);
        CHECK(sel_library_deferred() == d1); /* the last try ran, it was not deferred */
        CHECK(sel_library_watched_waits() == w1 + 1u);
    }
    /* I961 step 2: a tagged round trip lands in its own (site, request id) row. */
    {
        sel_message reply;
        uint32_t si, calls_before = 0, calls_after = 0, ok = 0, tries, rows;
        const char* site;
        uint32_t req, calls, ms;
        int nw = sel_library_nowait(1);
        const char* prev = sel_library_site("i961_probe");
        for (si = 0; sel_library_site_stat(si, &site, &req, &calls, &ms); si++) {
            if (strcmp(site, "i961_probe") == 0 && req == SEL_LIB_REQ_STATS) calls_before = calls;
        }
        for (tries = 0; tries < 2000 && !ok; tries++) {
            ok = (uint32_t)sel_library_call(lib, SEL_LIB_REQ_STATS, &reply);
            if (!ok) usleep(1000);
        }
        sel_library_site(prev);
        sel_library_nowait(nw);
        CHECK(ok);
        rows = 0;
        for (si = 0; sel_library_site_stat(si, &site, &req, &calls, &ms); si++) {
            if (strcmp(site, "i961_probe") == 0 && req == SEL_LIB_REQ_STATS) {
                calls_after = calls;
                rows++;
            }
        }
        CHECK(rows == 1u);
        CHECK(calls_after == calls_before + 1u);
    }
    /* I961c: SOURCES reads the writer-kept mirror. With the library FREE (the case I961 step 1 turned red) the command sends no worker
     * request: the served counter and the watched waits do not move, yet the counts and the sync time are right, a write included. */
    {
        sel_message reply;
        sel_meta_op op;
        uint32_t served0, w0;
        int64_t before;

        usleep(100000); /* let the posted reads of the checks above finish */
        op.key = "sync.local";
        op.value = "1234567";
        op.cap = 24;
        op.set = 1;
        memset(&reply, 0, sizeof reply);
        CHECK(sel_library_request(lib, SEL_LIB_REQ_META, &op, &reply) != 0); /* the write goes through the worker */
        served0 = lib->served;
        w0 = sel_library_watched_waits();
        before = sel_library_source_synced(lib, "local");
        CHECK(before == 1234567); /* read-after-write, no request */
        CHECK(sel_library_source_songs(lib, "local") == 2); /* the two songs seeded above, counted by the source_songs triggers */
        CHECK(sel_engine_send_line(e, "SOURCES", pool, sizeof pool) == 0);
        sel_engine_pump(e, sink, NULL);
        printf("i961c SOURCES: worker requests %u, watched waits %u\n", (unsigned)(lib->served - served0),
               (unsigned)(sel_library_watched_waits() - w0));
        CHECK(lib->served == served0);
        CHECK(sel_library_watched_waits() == w0);
    }
    sel_library_watch(was);

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) printf("library_ui_nowait_ok\n");
    return g_fail != 0;
}
