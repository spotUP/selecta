/* I906c: the commands desk_library_follow's reload (desk_page_load) sends never wait on the library worker. The desktop UI sends
 * them on every library change; with a sync job or scan batch holding the library call lock a blocking one froze the UI.
 *
 * Fixture: a thread owns the library call lock like a running sync job, for HOLD_MS. Through the real entry point
 * (sel_engine_send_line + sel_engine_pump) the test sends every command desk_page_load sends for the Home, Folders, Browse and
 * list pages, times each pump (the "sample": a blocking command shows as about HOLD_MS) and counts the synchronous library
 * requests it made (sel_library_watched_waits), and asserts
 *   - no command made a synchronous library request on the pump thread (the waits count; the pump times are printed, never asserted:
 *     a wall-clock budget flakes under load, I985),
 *   - after the release the posted reads land (stats.lib_reads_applied advanced: the sentinel that the posted path ran). */
#include <pthread.h>
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/plugin.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <selecta/registry.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "engine_test_db_lock.h"
#include "source.h"
#include "sources/modland_net.h"

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

/* I982: like holder, but the lock is let go when the test says so (g_release), not after a timer: the "nothing ran yet" checks of a queued write
 * are made while the lock is certainly still held, however long this thread was descheduled. The 20 s cap only frees a test that lost its main thread. */
static volatile int g_release;
static void* holder_gated(void* arg) {
    sel_library* lib = arg;
    struct timespec t0;

    pthread_mutex_lock(&lib->call_lock);
    g_held = 1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (!g_release && ms_since(&t0) < 20000) usleep(500);
    pthread_mutex_unlock(&lib->call_lock);
    return NULL;
}

/* I906f2e: a source whose sync hook waits for g_sync_go, then succeeds, so the test can hold the call lock before the pump collects it. */
static volatile int g_sync_go;

static int i906f2e_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    int waited = 0;

    (void)host;
    (void)arg;
    (void)now;
    (void)cap;
    while (!g_sync_go && waited++ < 5000) usleep(1000);
    stats[0].tag = SEL_TAG_END;
    stats[0].value = 0;
    return 1;
}

static const sel_source g_sync_src = {
    "i906f2esrc", "I906f2e sync", "", "Syncs on demand.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, i906f2e_sync, SEL_SOURCE_SYNCS, NULL, NULL,
    NULL,
};

/* I906f2f: like the I906f2e source, but its sync reports a "new this week" count and carries the flag, so sync_collect persists it. */
static int i906f2f_sync(const sel_host* host, const char* arg, int64_t now, sel_tag* stats, uint32_t cap) {
    int waited = 0;

    (void)host;
    (void)arg;
    (void)now;
    (void)cap;
    while (!g_sync_go && waited++ < 5000) usleep(1000);
    stats[0].tag = SEL_SYNC_NEW_THIS_WEEK;
    stats[0].value = 77;
    stats[1].tag = SEL_TAG_END;
    stats[1].value = 0;
    return 1;
}

static const sel_source g_ntw_src = {
    "i906f2fsrc", "I906f2f sync", "", "Syncs with a new this week count.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, i906f2f_sync,
    SEL_SOURCE_SYNCS | SEL_SOURCE_NEW_THIS_WEEK, NULL, NULL, NULL,
};

/* The id PLAYLIST_CREATE answers for the "addf" playlist (LIBRARY_RESULT key "playlist", name, value = id). */
static volatile int g_addf_plid;

static void sink(void* ctx, const sel_message* evt) {
    const char* key;
    const char* name;
    const char* value;

    (void)ctx;
    if (evt->id != SEL_EVT_LIBRARY_RESULT || evt->argc == 0 || evt->args[0].type != SEL_ARG_TAGS) return;
    key = (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_KEY, 0);
    name = (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_NAME, 0);
    value = (const char*)sel_tag_find(evt->args[0].v.t, SEL_SRC_PATH, 0);
    if (key != NULL && name != NULL && value != NULL && strcmp(key, "playlist") == 0 && strcmp(name, "addf") == 0) {
        g_addf_plid = atoi(value);
    }
}

/* I906f2: a source with a search of one hit, so SOURCE_PIN has a hit to pin. */
static int pin_search(const sel_host* host, const char* query, sel_search_sink* out) {
    (void)host;
    (void)query;
    return out->row(out->ctx, "hit1", "Hit One", "Hit Artist", "mod") ? 1 : 0;
}

static const sel_source g_pin_src = {
    "i906f2src", "I906f2 pin source", "", "One search hit.", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, pin_search,
};

/* I906f2c: a replayer of one loud constant tone (extension "lvl") so a song opens for real and the adaptive leveler measures it. */
static int g_lvl_state;
static uint32_t g_lvl_left; /* frames the song still has: it ends after LVL_FRAMES, which is what hands its slot back */
enum { LVL_FRAMES = 160000 };

static int lvl_probe(const uint8_t* data, size_t size, const char* name) {
    size_t n = name != NULL ? strlen(name) : 0;

    (void)data;
    (void)size;
    return n >= 4 && strcmp(name + n - 4, ".lvl") == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* lvl_open(const sel_song_source* src, const sel_host* h, sel_rp_info* info) {
    (void)src;
    (void)h;
    info->subsong_count = 1;
    info->default_subsong = 0;
    return &g_lvl_state;
}

static void lvl_close(void* state) {
    (void)state;
}

static int lvl_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    (void)state;
    (void)subsong;
    (void)rate;
    (void)voices;
    g_lvl_left = LVL_FRAMES;
    return 1;
}

static uint32_t lvl_render(void* state, int16_t* out, uint32_t frames) {
    uint32_t i;

    (void)state;
    if (frames > g_lvl_left) frames = g_lvl_left;
    g_lvl_left -= frames;
    for (i = 0; i < frames * 2u; i++) out[i] = (int16_t)((i / 2u) % 100u < 50u ? 12000 : -12000);
    return frames;
}

static sel_tag g_lvl_hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER}, {SEL_RP_EXTENSIONS, (uintptr_t) "lvl"}, {SEL_RP_PROBE, SEL_TAG_FN(lvl_probe)},
                                {SEL_RP_OPEN, SEL_TAG_FN(lvl_open)}, {SEL_RP_CLOSE, SEL_TAG_FN(lvl_close)}, {SEL_RP_START, SEL_TAG_FN(lvl_start)},
                                {SEL_RP_RENDER, SEL_TAG_FN(lvl_render)}, {SEL_TAG_END, 0}};
static sel_tag g_lvl_desc[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake_level"}, {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                               {SEL_PLUGIN_TAGS, (uintptr_t)g_lvl_hooks}, {SEL_TAG_END, 0}};

static uint32_t reg_count(void) {
    return sel_registry_count() + 1u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == sel_registry_count() ? g_lvl_desc : sel_registry_get(i);
}

int main(int argc, char** argv) {
    CHECK(argc >= 2); /* argv[1] = gen_testmod */
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
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    db_enter(lib);
    CHECK(sqlite3_exec(lib->db,
                       "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES"
                       "('local:a.mod','local','a.mod','a','Ann','Alb1','mod',100),('local:b.mod','local','b.mod','b','Ann','Alb1','mod',200),"
                       "('local:c.sid','local','c.sid','c','Bob',NULL,'sid',300);",
                       NULL, NULL, NULL) == SQLITE_OK);
    { /* I906f2c: the song row the leveler's provisional gain updates (key of the lvl file the test plays) */
        char sql[700];

        snprintf(sql, sizeof sql, "INSERT INTO songs(key, source, path, name, artist, format, added) VALUES('local:%s/level.lvl','local','%s/level.lvl','lvl','Ann','lvl',400);", dir, dir);
        CHECK(sqlite3_exec(lib->db, sql, NULL, NULL, NULL) == SQLITE_OK);
    }
    db_leave(lib);
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
        if (w1 != w0) {
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
            if (w1 != w0) {
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
    /* I906f2: the pin of a search hit (SOURCE_PIN) and the appends of a PLAYLIST_ADD_FOLDER scan are queued writes. Sentinel per write:
     * stats.lib_writes_done rises by one for it, and its counter (source_pins, addf_runs) moves in the done callback, not at the call. The pin
     * is also checked with the call lock held (nothing lands before the release, no synchronous request). */
    {
        sel_engine_stats p_before, p_after;
        char line[700], tree[300], keys[1024];
        int plid = 0;
        uint32_t nkeys = 0;

        CHECK(sel_source_register(&g_pin_src));
        CHECK(sel_engine_send_line(e, "SOURCE_SEARCH i906f2src q", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            if (ms_since(&t0) > 400) break; /* the search finishes in a few ms; the hit is in the table once it is collected */
            usleep(2000);
        }
        sel_engine_stats_get(e, &p_before);
        g_held = 0;
        g_release = 0;
        pthread_create(&th, NULL, holder_gated, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, "SOURCE_PIN \"i906f2src:hit1\"", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        printf("i906f2 pin: SOURCE_PIN pump %ld ms, library waits %u\n", pump_ms, (unsigned)(w1 - w0));
        CHECK(w1 == w0);
        sel_engine_stats_get(e, &p_after);
        CHECK(p_after.source_pins == p_before.source_pins); /* nothing ran yet: the pin is queued behind the held lock */
        g_release = 1; /* I982: let go only now */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &p_after);
            if (p_after.source_pins > p_before.source_pins) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2 pin after release: source_pins +%u, lib_writes_done +%u\n", (unsigned)(p_after.source_pins - p_before.source_pins),
               (unsigned)(p_after.lib_writes_done - p_before.lib_writes_done));
        CHECK(p_after.source_pins == p_before.source_pins + 1u);
        CHECK(p_after.lib_writes_done == p_before.lib_writes_done + 1u); /* the pin went through the write queue */

        /* add-folder: a tree with one module; the scan runs with the lock held, so addf_finish runs on the pump while it is held */
        snprintf(tree, sizeof tree, "%s/addf", dir);
        snprintf(line, sizeof line, "mkdir -p \"%s\" && \"%s\" \"%s/one.mod\"", tree, argv[1], tree);
        CHECK(system(line) == 0);
        CHECK(sel_engine_send_line(e, "PLAYLIST_CREATE addf", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            if (g_addf_plid != 0) break;
            if (ms_since(&t0) > 3000) break;
            usleep(2000);
        }
        /* The id comes from the PLAYLIST_CREATE answer, as a UI gets it. Not a direct SELECT on lib->db: the create's done callback posts a
         * folders reload (folders_reload_posted), so the worker is inside that request when the answer arrives, and a prepare here is two
         * threads in sqlite on one connection (library_thread_violation, owner_check in db/worker.c). */
        plid = g_addf_plid;
        CHECK(plid != 0);
        sel_engine_stats_get(e, &p_before);
        /* The scan itself needs the library, so the lock cannot be held across it; the sentinel is the queue: the appends run on the
         * write thread, so stats.lib_writes_done rises by one with the run, and the run lands after the pump that ended the scan. */
        snprintf(line, sizeof line, "PLAYLIST_ADD_FOLDER %d \"%s\"", plid, tree);
        waits0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        worst = 0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            struct timespec t1;

            clock_gettime(CLOCK_MONOTONIC, &t1);
            sel_engine_pump(e, sink, NULL);
            if (ms_since(&t1) > worst) worst = ms_since(&t1);
            sel_engine_stats_get(e, &p_after);
            if (p_after.addf_runs > p_before.addf_runs) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2 addf: worst pump %ld ms, pump-thread library waits %u, addf_runs +%u, addf_added %u, plfail %u/%u, lib_writes_done +%u\n", worst,
               (unsigned)(sel_library_watched_waits() - waits0),
               (unsigned)(p_after.addf_runs - p_before.addf_runs), (unsigned)p_after.addf_added,
               (unsigned)p_after.pl_fail_runs, (unsigned)p_after.pl_fail_reason, (unsigned)(p_after.lib_writes_done - p_before.lib_writes_done));
        /* I985: the wall-clock budget flaked under load. What it protected is that addf_finish and its done callback make no synchronous library
         * request on the pump thread (the appends go through the write queue, the folders reload is posted). Measured: a baseline LIBRARY_SCAN of
         * an identical tree makes 0 such requests, so the add-folder run must make 0 too. */
        CHECK(sel_library_watched_waits() == waits0);
        CHECK(p_after.addf_runs == p_before.addf_runs + 1u);
        CHECK(p_after.addf_added == 1u);
        CHECK(p_after.lib_writes_done == p_before.lib_writes_done + 1u); /* the appends went through the write queue */
        sel_library_watch(0);
        CHECK(sel_library_playlist_keys(lib, (uint32_t)plid, keys, sizeof keys, &nkeys) == 0 && nkeys == 1u);
        sel_library_watch(1);
    }
    /* I906f2b: the Recents touch of COLLECTION_OPEN is a queued write. Sentinel: stats.lib_writes_done rises by one for it and lib_touch_calls
     * moves in the done callback; neither moves while the call lock is held. The result is taken from the stats only: this thread runs no
     * sqlite while the worker may be busy. */
    {
        sel_engine_stats t_before, t_after;
        char line[200];

        sel_engine_stats_get(e, &t_before);
        g_held = 0;
        g_release = 0;
        pthread_create(&th, NULL, holder_gated, lib);
        while (!g_held) usleep(1000);
        snprintf(line, sizeof line, "COLLECTION_OPEN %u \"Touch Album\"", (unsigned)SEL_LIB_ITEM_ALBUM);
        w0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        CHECK(w1 == w0);
        sel_engine_stats_get(e, &t_after);
        CHECK(t_after.lib_touch_calls == t_before.lib_touch_calls); /* nothing ran yet: the touch is queued behind the held lock */
        g_release = 1; /* I982: let go only now */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &t_after);
            if (t_after.lib_touch_calls > t_before.lib_touch_calls) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2b touch: pump %ld ms, lib_touch_calls +%u, lib_writes_done +%u\n", pump_ms,
               (unsigned)(t_after.lib_touch_calls - t_before.lib_touch_calls), (unsigned)(t_after.lib_writes_done - t_before.lib_writes_done));
        CHECK(t_after.lib_touch_calls == t_before.lib_touch_calls + 1u);
        CHECK(t_after.lib_writes_done == t_before.lib_writes_done + 1u); /* the touch went through the write queue */
    }
    /* I906f2c: the provisional gain the adaptive leveler measured is a queued write, taken when the finished slot is reclaimed on the pump.
     * A loud tone plays past the 3 s gate in auto mode, then the song ends (it is 160000 frames long) while the call lock is held: the pump returns fast with no
     * synchronous request and stats.leveler_writes has not moved. After the release leveler_writes and lib_writes_done each rise by one. */
    {
        sel_engine_stats v_before, v_after;
        char path[512], line[600];
        int16_t buf[4410u * 2u];
        uint32_t k, opened0, opened1;
        FILE* f;

        snprintf(path, sizeof path, "%s/level.lvl", dir);
        f = fopen(path, "wb");
        CHECK(f != NULL);
        if (f != NULL) {
            fputs("x", f);
            fclose(f);
        }
        CHECK(sel_engine_send_line(e, "PREF_SET mix.loudness \"auto\"", pool, sizeof pool) == 0);
        sel_engine_pump(e, sink, NULL); /* the pool is reused by the next line: take this one first */
        snprintf(line, sizeof line, "OPEN \"%s\"", path);
        sel_engine_stats_get(e, &v_before);
        opened0 = v_before.songs_opened;
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        /* I981: the start waits for its library read (I906g) on another thread, so the song is open when songs_opened rises, not after a fixed pump count */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        do {
            sel_engine_pump(e, sink, NULL);
            sel_engine_render(e, buf, 441u); /* this thread is the audio side: a render hands a busy slot back so the start can take it */
            sel_engine_stats_get(e, &v_before);
            if (v_before.songs_opened > opened0) break;
            usleep(2000);
        } while (ms_since(&t0) < 5000);
        CHECK(v_before.songs_opened > opened0);
        CHECK(sel_engine_send_line(e, "PLAY", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) { /* render until the leveler has measured more than its 3 s gate */
            sel_engine_pump(e, sink, NULL);
            sel_engine_render(e, buf, 4410u);
            sel_engine_stats_get(e, &v_before);
            if (v_before.leveler_frames > 150000u || ms_since(&t0) > 5000) break;
        }
        sel_engine_pump(e, sink, NULL);
        sel_engine_stats_get(e, &v_before);
        printf("i906f2c state %d leveler_frames %u source %s\n", (int)sel_engine_state(e)->play, (unsigned)v_before.leveler_frames, v_before.loudness_source);
        CHECK(v_before.leveler_frames > 0u); /* the leveler measured the tone */
        opened1 = v_before.songs_opened;
        snprintf(line, sizeof line, "OPEN \"%s\"", path); /* replaces the song: the swap is queued for the audio side, the old slot is handed back by the next render */
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        do { /* pump only (no render) until the new song has loaded and its swap waits (I981: wait on the counter, not on a pump count) */
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &v_after);
            if (v_after.songs_opened > opened1) break;
            usleep(2000);
        } while (ms_since(&t0) < 5000);
        CHECK(v_after.songs_opened > opened1);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_render(e, buf, 4410u); /* pops the swap: the leveled slot goes to the return queue */
        for (k = 0; k < 5u; k++) { /* the pump reclaims it: the old code wrote the gain synchronously and waited on the held lock here */
            sel_engine_pump(e, sink, NULL);
            sel_engine_render(e, buf, 441u);
        }
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        sel_engine_stats_get(e, &v_after);
        CHECK(w1 == w0);
        CHECK(v_after.leveler_writes == v_before.leveler_writes); /* queued behind the held lock */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_render(e, buf, 441u); /* this thread is the audio side too: renders hand the stopped slot back */
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &v_after);
            if (v_after.leveler_writes > v_before.leveler_writes) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2c leveler: pump %ld ms, waits %u, leveler_writes +%u, lib_writes_done +%u\n", pump_ms, (unsigned)(w1 - w0),
               (unsigned)(v_after.leveler_writes - v_before.leveler_writes), (unsigned)(v_after.lib_writes_done - v_before.lib_writes_done));
        CHECK(v_after.leveler_writes == v_before.leveler_writes + 1u);
        CHECK(v_after.lib_writes_done >= v_before.lib_writes_done + 1u); /* the write queue ran (the song end queues its history row too) */
    }
    /* I906f2d: VIEW_SAVE is a queued write. Sentinel: stats.lib_writes_done rises by one for it; the view row is absent while the call lock is
     * held and present (read back from the views table) after the release. */
    {
        sel_engine_stats s_before, s_after;
        sqlite3_stmt* st = NULL;
        char qtext[64] = "";
        int found = 0;

        sel_engine_stats_get(e, &s_before);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, "VIEW_SAVE i906f2dview format:xm", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        sel_engine_stats_get(e, &s_after);
        CHECK(w1 == w0);
        CHECK(s_after.lib_writes_done == s_before.lib_writes_done); /* queued behind the held lock */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &s_after);
            if (s_after.lib_writes_done > s_before.lib_writes_done) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2d view: pump %ld ms, waits %u, lib_writes_done +%u\n", pump_ms, (unsigned)(w1 - w0),
               (unsigned)(s_after.lib_writes_done - s_before.lib_writes_done));
        CHECK(s_after.lib_writes_done == s_before.lib_writes_done + 1u); /* the save went through the write queue */
        sel_library_watch(0);
        db_enter(lib);
        if (sqlite3_prepare_v2(lib->db, "SELECT query FROM views WHERE name = 'i906f2dview'", -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) {
                found = 1;
                snprintf(qtext, sizeof qtext, "%s", (const char*)sqlite3_column_text(st, 0));
            }
            sqlite3_finalize(st);
        }
        db_leave(lib);
        sel_library_watch(1);
        CHECK(found && strcmp(qtext, "format:xm") == 0);
    }
    /* I906f2e: the sync time stamp (sync_collect on the pump) is a queued write. The sync finishes, then the call lock is taken before the pump
     * collects it: the collecting pump stays fast, makes no watched wait, and lib_writes_done moves only after the release (the sentinel);
     * the stamp is read back from the meta table. */
    {
        sel_engine_stats s_before, s_after;
        sqlite3_stmt* st = NULL;
        long long stored = 0;

        CHECK(sel_source_register(&g_sync_src));
        sel_engine_stats_get(e, &s_before);
        CHECK(sel_engine_send_line(e, "LIBRARY_SYNC i906f2esrc /x 1234567", pool, sizeof pool) == 0);
        sel_engine_pump(e, sink, NULL);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        g_sync_go = 1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        worst = 0;
        for (i = 0; i < 300; i++) { /* the job ends, the collecting pump queues the stamp */
            struct timespec tp;

            clock_gettime(CLOCK_MONOTONIC, &tp);
            sel_engine_pump(e, sink, NULL);
            if (ms_since(&tp) > worst) worst = ms_since(&tp);
            usleep(2000);
        }
        w1 = sel_library_watched_waits();
        sel_engine_stats_get(e, &s_after);
        printf("i906f2e sync time: worst pump %ld ms, waits %u\n", worst, (unsigned)(w1 - w0));
        CHECK(w1 == w0);
        CHECK(s_after.lib_writes_done == s_before.lib_writes_done); /* queued behind the held lock */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &s_after);
            if (s_after.lib_writes_done > s_before.lib_writes_done) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2e after release: lib_writes_done +%u\n", (unsigned)(s_after.lib_writes_done - s_before.lib_writes_done));
        CHECK(s_after.lib_writes_done == s_before.lib_writes_done + 1u);
        sel_library_watch(0);
        db_enter(lib);
        if (sqlite3_prepare_v2(lib->db, "SELECT value FROM meta WHERE key = 'sync.i906f2esrc'", -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) stored = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
        db_leave(lib);
        sel_library_watch(1);
        CHECK(stored == 1234567);
    }
    /* I906f2f: persist_new_this_week (sync_collect on the pump) is a queued write. Same shape as I906f2e; the finished sync queues two writes
     * (the sync stamp and the count), so lib_writes_done moves by 2 after the release. The count is read back from the meta table. */
    {
        sel_engine_stats s_before, s_after;
        sqlite3_stmt* st = NULL;
        long long stored = 0;

        CHECK(sel_source_register(&g_ntw_src));
        g_sync_go = 0;
        sel_engine_stats_get(e, &s_before);
        CHECK(sel_engine_send_line(e, "LIBRARY_SYNC i906f2fsrc /x 1234567", pool, sizeof pool) == 0);
        sel_engine_pump(e, sink, NULL);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        g_sync_go = 1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        worst = 0;
        for (i = 0; i < 300; i++) {
            struct timespec tp;

            clock_gettime(CLOCK_MONOTONIC, &tp);
            sel_engine_pump(e, sink, NULL);
            if (ms_since(&tp) > worst) worst = ms_since(&tp);
            usleep(2000);
        }
        w1 = sel_library_watched_waits();
        sel_engine_stats_get(e, &s_after);
        printf("i906f2f new this week: worst pump %ld ms, waits %u\n", worst, (unsigned)(w1 - w0));
        CHECK(w1 == w0);
        CHECK(s_after.lib_writes_done == s_before.lib_writes_done);
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &s_after);
            if (s_after.lib_writes_done >= s_before.lib_writes_done + 2u) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2f after release: lib_writes_done +%u\n", (unsigned)(s_after.lib_writes_done - s_before.lib_writes_done));
        CHECK(s_after.lib_writes_done == s_before.lib_writes_done + 2u);
        sel_library_watch(0);
        db_enter(lib);
        if (sqlite3_prepare_v2(lib->db, "SELECT value FROM meta WHERE key = '" SEL_MODLAND_META_NEW "'", -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) stored = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
        db_leave(lib);
        sel_library_watch(1);
        CHECK(stored == 77);
    }
    /* I906f2g: VOTE writes the local sticker "vote" as a queued write (the readable-file check and the 32-character token sit before it). Sentinel:
     * stats.lib_writes_done rises with it and the sticker is absent while the call lock is held, read back as "5" after the release. */
    {
        sel_engine_stats vt_before, vt_after;
        char path[512], line[600], key[600], got[16] = "";
        uint32_t opened0;
        int16_t buf[441u * 2u];
        FILE* f;

        snprintf(path, sizeof path, "%s/vote.lvl", dir);
        f = fopen(path, "wb");
        CHECK(f != NULL);
        if (f != NULL) {
            fputs("vote-file", f);
            fclose(f);
        }
        snprintf(line, sizeof line, "OPEN \"%s\"", path);
        sel_engine_stats_get(e, &vt_before);
        opened0 = vt_before.songs_opened;
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        do {
            sel_engine_pump(e, sink, NULL);
            sel_engine_render(e, buf, 441u);
            sel_engine_stats_get(e, &vt_before);
            if (vt_before.songs_opened > opened0) break;
            usleep(2000);
        } while (ms_since(&t0) < 5000);
        CHECK(vt_before.songs_opened > opened0);
        for (clock_gettime(CLOCK_MONOTONIC, &t0); ms_since(&t0) < 400;) { /* let the follow-ups of the song start (panel, history) land before the lock is held */
            sel_engine_pump(e, sink, NULL);
            sel_engine_render(e, buf, 441u);
            usleep(2000);
        }
        sel_engine_stats_get(e, &vt_before);
        /* The first VOTE creates the vote token (a prefs write, which changes the prefs generation and so makes the pump's smart_sync
         * request the library: a separate wait, not the vote). Settle it with the lock free, then vote again under the held lock. */
        CHECK(sel_engine_send_line(e, "VOTE 5", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            if (ms_since(&t0) > 800) break;
            usleep(2000);
        }
        sel_engine_stats_get(e, &vt_before);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        CHECK(sel_engine_send_line(e, "VOTE 0", pool, sizeof pool) == 0);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_engine_pump(e, sink, NULL);
        pump_ms = ms_since(&t0);
        w1 = sel_library_watched_waits();
        sel_engine_stats_get(e, &vt_after);
        printf("i906f2g vote: pump %ld ms, waits %u\n", pump_ms, (unsigned)(w1 - w0));
        CHECK(w1 == w0);
        CHECK(vt_after.lib_writes_done == vt_before.lib_writes_done); /* queued behind the held lock */
        pthread_join(th, NULL);
        snprintf(key, sizeof key, "local:%s", path);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &vt_after);
            if (vt_after.lib_writes_done > vt_before.lib_writes_done) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        CHECK(vt_after.lib_writes_done >= vt_before.lib_writes_done + 1u); /* the sticker went through the write queue (a vote count may add one) */
        sel_library_watch(0);
        CHECK(sel_library_sticker_get(lib, key, "vote", got, sizeof got) != 0 && strcmp(got, "0") == 0);
        sel_library_watch(1);
    }
    /* I906f2g: ANALYSE stores the gain through the write queue. Sentinel: stats.analyse_jobs and lib_writes_done move only after the release;
     * the gain is read back from the loudness table. The render of the private engine is CPU work, not a library wait: watched waits stay 0. */
    {
        sel_engine_stats an_before, an_after;
        char path[512], line[600], key[600];
        int32_t gain = 0;
        FILE* f;

        snprintf(path, sizeof path, "%s/level.lvl", dir);
        f = fopen(path, "wb");
        CHECK(f != NULL);
        if (f != NULL) {
            fputs("x", f);
            fclose(f);
        }
        snprintf(key, sizeof key, "local:%s", path);
        for (clock_gettime(CLOCK_MONOTONIC, &t0); ms_since(&t0) < 400;) { /* the vote service answer and the song follow-ups land first */
            sel_engine_pump(e, sink, NULL);
            usleep(2000);
        }
        sel_engine_stats_get(e, &an_before);
        g_held = 0;
        pthread_create(&th, NULL, holder, lib);
        while (!g_held) usleep(1000);
        w0 = sel_library_watched_waits();
        snprintf(line, sizeof line, "ANALYSE \"%s\"", path);
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        sel_engine_pump(e, sink, NULL);
        w1 = sel_library_watched_waits();
        sel_engine_stats_get(e, &an_after);
        CHECK(w1 == w0);
        CHECK(an_after.analyse_jobs == an_before.analyse_jobs);
        CHECK(an_after.lib_writes_done == an_before.lib_writes_done); /* queued behind the held lock */
        pthread_join(th, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (;;) {
            sel_engine_pump(e, sink, NULL);
            sel_engine_stats_get(e, &an_after);
            if (an_after.analyse_jobs > an_before.analyse_jobs) break;
            if (ms_since(&t0) > 5000) break;
            usleep(2000);
        }
        printf("i906f2g analyse: analyse_jobs +%u, lib_writes_done +%u, gain_cdb %d\n", (unsigned)(an_after.analyse_jobs - an_before.analyse_jobs),
               (unsigned)(an_after.lib_writes_done - an_before.lib_writes_done), (int)an_after.analyse_gain_cdb);
        CHECK(an_after.analyse_jobs == an_before.analyse_jobs + 1u);
        CHECK(an_after.lib_writes_done == an_before.lib_writes_done + 1u);
        sel_library_watch(0);
        CHECK(sel_library_loudness_get(lib, key, &gain) != 0 && gain == an_after.analyse_gain_cdb);
        sel_library_watch(1);
    }
    sel_library_watch(was);

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) printf("library_ui_follow_reads_ok\n");
    return g_fail != 0;
}
