/* I906c: the commands desk_library_follow's reload (desk_page_load) sends never wait on the library worker. The desktop UI sends
 * them on every library change; with a sync job or scan batch holding the library call lock a blocking one froze the UI.
 *
 * Fixture: a thread owns the library call lock like a running sync job, for HOLD_MS. Through the real entry point
 * (sel_engine_send_line + sel_engine_pump) the test sends every command desk_page_load sends for the Home, Folders, Browse and
 * list pages, times each pump (the "sample": a blocking command shows as about HOLD_MS) and counts the synchronous library
 * requests it made (sel_library_watched_waits), and asserts
 *   - every pump returns within PUMP_BUDGET_MS, and no command made a synchronous library request (Debug builds),
 *   - after the release the posted reads land (stats.lib_reads_applied advanced: the sentinel that the posted path ran). */
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
    static const char* const lines[] = {"VIEW_SOURCE home", "VIEW_WINDOW 12", "VIEW_SHOW browse", "COMPOSERS_SHOW", "STATS_SHOW",
                                        "FORMATS_SHOW",     "SOURCES",        "VIEW_SOURCE folders", "VIEW_WINDOW 64", "LIBRARY_SEARCH \"\""};
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_followreads_XXXXXX";
    char db[256], pool[1024];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    pthread_t th;
    struct timespec t0;
    sel_engine_stats s0, s1;
    long pump_ms, worst = 0;
    uint32_t i, w0, w1, waits0, waits;
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
                       "('local:c.sid','local','c.sid','c','Bob',NULL,'sid',300);",
                       NULL, NULL, NULL) == SQLITE_OK);
    sel_engine_stats_get(e, &s0);

    pthread_create(&th, NULL, holder, lib);
    while (!g_held) usleep(1000);
    was = sel_library_watch(1); /* this thread is the pump thread: count every library request it makes */
    waits0 = sel_library_watched_waits();
    for (i = 0; i < sizeof lines / sizeof lines[0]; i++) {
        w0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, lines[i], pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        if (pump_ms > worst) worst = pump_ms;
        printf("i906c sample: '%s' pump %ld ms, library waits %u\n", lines[i], pump_ms, (unsigned)(w1 - w0));
        if (pump_ms >= PUMP_BUDGET_MS || w1 != w0) {
            printf("i906c FAIL: '%s' waited on the library\n", lines[i]);
            g_fail++;
        }
    }
    waits = sel_library_watched_waits() - waits0;
    printf("i906c busy library: worst pump %ld ms (budget %d), synchronous library waits on the pump thread %u\n", worst, PUMP_BUDGET_MS,
           (unsigned)waits);
    pthread_join(th, NULL);

    /* The library is free: the posted reads finish and their answers arrive through the pump. */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        sel_engine_pump(e, sink, NULL);
        sel_engine_stats_get(e, &s1);
        if (s1.lib_reads_applied >= s0.lib_reads_applied + 2u && sel_engine_state(e)->stats != NULL) break; /* the stats answer may land after the others */
        if (ms_since(&t0) > 5000) break;
        usleep(2000);
    }
    printf("i906c after release: async reads applied %u\n", (unsigned)(s1.lib_reads_applied - s0.lib_reads_applied));
    CHECK(s1.lib_reads_applied >= s0.lib_reads_applied + 2u);
    CHECK(sel_engine_state(e)->stats != NULL); /* STATS_SHOW's answer landed through its slot */

    /* I906f: the library writes the UI sends (like, rate, folder and playlist edits, stickers) are queued to the writer thread. Same fixture:
     * the call lock is held, every write must return from its pump within the budget with no synchronous request, and after the release all
     * of them land (stats.lib_writes_done is the sentinel that the queue ran, the like is read back from the library). */
    {
        static const char* const writes[] = {"LIKE local:a.mod",
                                             "RATE 4 local:a.mod",
                                             "FOLDER_ADD Rock",
                                             "FOLDER_MOVE 1 0",
                                             "FOLDER_DEL 99",
                                             "PLAYLIST_MOVE 1 0",
                                             "PLAYLIST_ADD_SONG 1 local:a.mod",
                                             "PLAYLIST_REMOVE_SONG 1 1",
                                             "PLAYLIST_RENAME 1 Mix",
                                             "STICKER_SET local:a.mod mood happy",
                                             "STICKER_DEL local:a.mod mood"};
        const uint32_t n = (uint32_t)(sizeof writes / sizeof writes[0]);
        sel_engine_stats w_before, w_after;

        sel_engine_stats_get(e, &w_before);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        worst = 0;
        for (i = 0; i < n; i++) {
            w0 = sel_library_watched_waits();
            CHECK(sel_engine_send_line(e, writes[i], pool, sizeof pool) == 0);
            clock_gettime(CLOCK_MONOTONIC, &t0);
            sel_engine_pump(e, sink, NULL);
            pump_ms = ms_since(&t0);
            w1 = sel_library_watched_waits();
            if (pump_ms > worst) worst = pump_ms;
            printf("i906f sample: '%s' pump %ld ms, library waits %u\n", writes[i], pump_ms, (unsigned)(w1 - w0));
            if (pump_ms >= PUMP_BUDGET_MS || w1 != w0) {
                printf("i906f FAIL: '%s' waited on the library\n", writes[i]);
                g_fail++;
            }
        }
        printf("i906f busy library: worst write pump %ld ms (budget %d)\n", worst, PUMP_BUDGET_MS);
        sel_engine_stats_get(e, &w_after);
        CHECK(w_after.lib_writes_done == w_before.lib_writes_done); /* nothing ran yet: the worker is blocked */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &w_after);
            if (w_after.lib_writes_done >= w_before.lib_writes_done + n) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f after release: queued writes done %u of %u\n", (unsigned)(w_after.lib_writes_done - w_before.lib_writes_done), (unsigned)n);
        CHECK(w_after.lib_writes_done == w_before.lib_writes_done + n);
        sel_library_watch(0);
        CHECK(sel_library_liked(lib, "local:a.mod") == 1); /* the like landed in the library */
        sel_library_watch(1);
    }
    /* I906g: the start of a song reads its best source, per-song config and loudness first. With the call lock held the OPEN must return from
     * its pump within the budget with no synchronous request (the start waits, like it waits for a free slot); after the release the posted
     * read lands (stats.play_meta_reads is the sentinel that it ran). */
    {
        sel_engine_stats g_before, g_after;

        sel_engine_stats_get(e, &g_before);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, "OPEN \"local:a.mod\"", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        printf("i906g sample: OPEN pump %ld ms, library waits %u\n", pump_ms, (unsigned)(w1 - w0));
        CHECK(pump_ms < PUMP_BUDGET_MS);
        CHECK(w1 == w0);
        sel_engine_stats_get(e, &g_after);
        CHECK(g_after.play_meta_reads == g_before.play_meta_reads); /* nothing ran yet: the worker is blocked */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &g_after);
            if (g_after.play_meta_reads > g_before.play_meta_reads) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906g after release: play_meta_reads %u\n", (unsigned)(g_after.play_meta_reads - g_before.play_meta_reads));
        CHECK(g_after.play_meta_reads > g_before.play_meta_reads);
    }
    /* I906f group 1: the history row and the cue point of the song being played are queued writes. A song is opened first (library free), then
     * the call lock is held: CUE_SET and the replacing OPEN (which ends the song, hence the history row) must return within the budget with no
     * synchronous request; after the release stats.cue_sets and stats.history_writes rise (sentinels). */
    {
        sel_engine_stats h_before, h_after;
        char open_line[512];

        snprintf(open_line, sizeof open_line, "OPEN \"%s\"", SELECTA_TEST_MOD);
        CHECK(sel_engine_send_line(e, open_line, pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            if (ms_since(&t0) > 300) break;
            usleep(2000);
        }
        printf("i906f group1: song state %d\n", (int)sel_engine_state(e)->play);
        sel_engine_stats_get(e, &h_before); /* no player in this registry: the opened song keeps its history key, which is all the two writes need */
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, "CUE_SET 1", pool, sizeof pool) == 0);
        CHECK(sel_engine_send_line(e, open_line, pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        printf("i906f group1: CUE_SET + replace pump %ld ms, library waits %u\n", pump_ms, (unsigned)(w1 - w0));
        CHECK(pump_ms < PUMP_BUDGET_MS);
        CHECK(w1 == w0);
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &h_after);
            if (h_after.cue_sets > h_before.cue_sets && h_after.history_writes > h_before.history_writes) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f group1 after release: cue_sets +%u, history_writes +%u\n", (unsigned)(h_after.cue_sets - h_before.cue_sets),
               (unsigned)(h_after.history_writes - h_before.history_writes));
        CHECK(h_after.cue_sets == h_before.cue_sets + 1u);
        CHECK(h_after.history_writes > h_before.history_writes);
    }
    sel_library_watch(was);

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) printf("library_ui_follow_reads_ok\n");
    return g_fail != 0;
}
