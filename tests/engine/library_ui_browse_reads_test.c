/* I906b: FORMATS_SHOW and COMPOSERS_SHOW (the Browse and Library pages) never wait on the library worker. Both ran their
 * group queries on the pump thread through a blocking library call; with a sync job or scan batch holding the library call
 * lock the UI froze. They now post a read slot (rd_formats, rd_composers) and the answer lands through lib_reads_collect.
 *
 * Fixture: a thread owns the library call lock like a running sync job, for HOLD_MS. Through the real entry point
 * (sel_engine_send_line + sel_engine_pump) the test sends FORMATS_SHOW, COMPOSERS_SHOW, COMPOSERS_SHOW with a name and
 * COMPOSERS_VIEW, and asserts
 *   - every pump returns within PUMP_BUDGET_MS while the library is busy,
 *   - sel_library_watched_waits() counts 0 synchronous library requests from the pump thread (Debug builds; 0 in Release),
 *   - the state carries no formats and no composers yet (the sentinel: nothing was computed on the pump thread),
 *   - after the release the answers land: state.formats, state.composers and state.composer_page hold the fixture rows and
 *     stats.lib_reads_applied advanced by the reads that ran (the sentinel that the posted path ran). */
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

static long ms_since(const struct timespec* t0) {
    struct timespec t1;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (long)((t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_nsec - t0->tv_nsec) / 1000000L);
}

/* Plays the sync job: owns the library call lock for HOLD_MS, then lets go. */
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
    (void)evt;
}

int main(void) {
    static const char* const lines[] = {"FORMATS_SHOW", "COMPOSERS_SHOW", "COMPOSERS_SHOW x", "COMPOSERS_VIEW \"\" 0"};
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_browsereads_XXXXXX";
    char db[256], pool[1024];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    pthread_t th;
    struct timespec t0;
    sel_engine_stats s0, s1;
    const sel_state* st;
    long pump_ms, worst = 0;
    uint32_t i, waits0, waits;
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
                       "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES"
                       "('local:a.mod','local','a.mod','a','Ann','Alb1','mod',100),('local:b.mod','local','b.mod','b','Ann','Alb1','mod',200),"
                       "('local:c.sid','local','c.sid','c','Bob',NULL,'sid',300);"
                       "INSERT INTO song_composers(song_key, canon, display) VALUES('local:a.mod','x','X'),('local:b.mod','x','X'),"
                       "('local:c.sid','y','Y');",
                       NULL, NULL, NULL) == SQLITE_OK);
    sel_engine_stats_get(e, &s0);
    st = sel_engine_state(e);

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
        if (pump_ms >= PUMP_BUDGET_MS) printf("i906b slow pump: '%s' took %ld ms\n", lines[i], pump_ms);
    }
    waits = sel_library_watched_waits() - waits0;
    printf("i906b busy library: worst pump %ld ms (budget %d), synchronous library waits on the pump thread %u\n", worst,
           PUMP_BUDGET_MS, (unsigned)waits);
    CHECK(worst < PUMP_BUDGET_MS);
    CHECK(waits == 0u);
    sel_engine_stats_get(e, &s1);
    CHECK(s1.lib_reads_applied == s0.lib_reads_applied); /* nothing landed while the library was held: nothing ran on the pump */
    CHECK(st->formats == NULL);
    CHECK(st->composer_page == NULL);
    pthread_join(th, NULL);

    /* The library is free: the posted reads finish and their answers arrive through the pump. */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        sel_engine_pump(e, sink, NULL);
        sel_engine_stats_get(e, &s1);
        if (st->formats != NULL && st->composers != NULL && st->composer_page != NULL) break;
        if (ms_since(&t0) > 5000) break;
        usleep(2000);
    }
    sel_engine_stats_get(e, &s1);
    printf("i906b after release: async reads applied %u\n", (unsigned)(s1.lib_reads_applied - s0.lib_reads_applied));
    CHECK(s1.lib_reads_applied >= s0.lib_reads_applied + 2u); /* the formats read and the composers read, each through its slot */
    if (st->formats != NULL) printf("i906b formats first=[%s] count=[%s]\n", st->formats, st->formats + strlen(st->formats) + 1u);
    CHECK(st->formats != NULL && strcmp(st->formats, "mod") == 0 && strcmp(st->formats + 4, "2") == 0);
    CHECK(st->composers != NULL && strcmp(st->composers, "x") == 0);
    CHECK(st->composer_page != NULL && strcmp(st->composer_page, "x") == 0);
    CHECK(sel_library_watched_waits() - waits0 == 0u);
    sel_library_watch(was);

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) printf("library_ui_browse_reads_ok\n");
    return g_fail != 0;
}
