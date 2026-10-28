/* I632b: the SONGS_* bulk verbs resolve a whole selection against the engine's own list. Driven through the engine command
 * entry (send + pump); songs_runs / songs_done / songs_stale in sel_engine_stats are the sentinel that the bulk code ran.
 * Covers: Play Next of 3 keeps list order, one UNDO restores the queue, one QUEUE_CHANGED per verb, a stale version changes
 * nothing, SONGS_PLAYLIST_ADD adds 3, playlist removal of {1,3} removes the right two, 70 songs pass (inline and staged by
 * SONGS_RANGES). */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"

static int g_fail;
static uint32_t g_qchanged;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_QUEUE_CHANGED) g_qchanged++;
}

static void run(sel_engine* eng, const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, sink, NULL);
}

static sel_engine_stats stats(sel_engine* e) {
    sel_engine_stats s;
    sel_engine_stats_get(e, &s);
    return s;
}

static const char* qpath(sel_engine* e, uint32_t i) {
    const sel_state* st = sel_engine_state(e);
    return st->queue_text + st->queue[i].path_offset;
}

static uint32_t qcount(sel_engine* e) { return sel_engine_state(e)->queue_count; }
static uint32_t qversion(sel_engine* e) { return sel_engine_state(e)->queue_version; }
static uint32_t vversion(sel_engine* e) { return sel_engine_state(e)->view->version; }

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_songsbulk_XXXXXX";
    char db[256], line[256], cmd[300], keys[4096];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    uint32_t n = 0, v, i, before, runs;
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

    run(e, "PLAYLIST_CREATE alpha"); /* saves the (empty) queue: playlist 1 starts empty */
    for (i = 0; i < 5; i++) {
        snprintf(line, sizeof line, "QUEUE_ADD \"/m/s%u.mod\"", (unsigned)i);
        run(e, line);
    }
    CHECK(qcount(e) == 5u);
    CHECK(stats(e).songs_runs == 0u);

    /* Play Next of three rows keeps list order and is one history step and one QUEUE_CHANGED. */
    v = qversion(e);
    g_qchanged = 0;
    snprintf(line, sizeof line, "SONGS_PLAY_NEXT queue %u \"1,3-4\"", (unsigned)v);
    run(e, line);
    CHECK(stats(e).songs_runs == 1u && stats(e).songs_done == 3u);
    CHECK(g_qchanged == 1u);
    CHECK(qcount(e) == 8u);
    CHECK(strcmp(qpath(e, 0), "/m/s1.mod") == 0 && strcmp(qpath(e, 1), "/m/s3.mod") == 0 && strcmp(qpath(e, 2), "/m/s4.mod") == 0);
    CHECK(strcmp(qpath(e, 3), "/m/s0.mod") == 0);
    run(e, "UNDO"); /* one UNDO restores the queue */
    CHECK(qcount(e) == 5u && strcmp(qpath(e, 0), "/m/s0.mod") == 0 && strcmp(qpath(e, 4), "/m/s4.mod") == 0);

    /* A stale version changes nothing. */
    g_qchanged = 0;
    snprintf(line, sizeof line, "SONGS_PLAY_NEXT queue %u \"0-2\"", (unsigned)qversion(e) - 1u);
    runs = stats(e).songs_runs;
    run(e, line);
    CHECK(qcount(e) == 5u && g_qchanged == 0u);
    CHECK(stats(e).songs_stale == 1u && stats(e).songs_runs == runs);
    /* A range past the end changes nothing either. */
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD queue %u \"3-9\"", (unsigned)qversion(e));
    run(e, line);
    CHECK(qcount(e) == 5u && stats(e).songs_runs == runs);

    /* SONGS_PLAYLIST_ADD adds 3, then 2 more. */
    snprintf(line, sizeof line, "SONGS_PLAYLIST_ADD 1 queue %u \"0-2\"", (unsigned)qversion(e));
    run(e, line);
    CHECK(stats(e).songs_done == 3u);
    CHECK(sel_library_playlist_keys(lib, 1u, keys, sizeof keys, &n) == 0 && n == 3u);
    snprintf(line, sizeof line, "SONGS_PLAYLIST_ADD 1 queue %u \"3,4\"", (unsigned)qversion(e));
    run(e, line);
    CHECK(sel_library_playlist_keys(lib, 1u, keys, sizeof keys, &n) == 0 && n == 5u);

    /* Removal of playlist positions {1,3} removes the right two. */
    run(e, "VIEW_SOURCE folders"); /* the Playlists page is showing: the view version it reads is current */
    snprintf(line, sizeof line, "SONGS_REMOVE playlist:1 %u \"1,3\"", (unsigned)vversion(e));
    run(e, line);
    CHECK(stats(e).songs_done == 2u);
    CHECK(sel_library_playlist_keys(lib, 1u, keys, sizeof keys, &n) == 0 && n == 3u);
    {
        const char* k = keys;
        CHECK(strcmp(k, "/m/s0.mod") == 0);
        k += strlen(k) + 1u;
        CHECK(strcmp(k, "/m/s2.mod") == 0);
        k += strlen(k) + 1u;
        CHECK(strcmp(k, "/m/s4.mod") == 0);
    }

    /* Queue removal of {0,2}: one history step, one event. */
    g_qchanged = 0;
    snprintf(line, sizeof line, "SONGS_REMOVE queue %u \"0,2\"", (unsigned)qversion(e));
    run(e, line);
    CHECK(qcount(e) == 3u && g_qchanged == 1u && strcmp(qpath(e, 0), "/m/s1.mod") == 0 && strcmp(qpath(e, 1), "/m/s3.mod") == 0);
    run(e, "UNDO");
    CHECK(qcount(e) == 5u);

    /* 70 songs pass: inline, and staged by SONGS_RANGES for a selection longer than one command line. */
    for (i = 0; i < 70; i++) {
        snprintf(line, sizeof line, "QUEUE_ADD \"/m/t%u.mod\"", (unsigned)i);
        run(e, line);
    }
    before = qcount(e);
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD queue %u \"5-74\"", (unsigned)qversion(e));
    run(e, line);
    CHECK(stats(e).songs_done == 70u && qcount(e) == before + 70u);
    before = qcount(e);
    run(e, "SONGS_RANGES \"0-9,20-29\"");
    run(e, "SONGS_RANGES \"40-49\"");
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD queue %u \"60-69\"", (unsigned)qversion(e));
    run(e, line);
    CHECK(stats(e).songs_done == 40u && qcount(e) == before + 40u);

    sel_engine_destroy(e);
    sel_library_close(lib);
    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", dir);
    (void)system(cmd);
    if (g_fail) {
        printf("songs_bulk: %d failures\n", g_fail);
        return 1;
    }
    printf("songs_bulk_ok\n");
    return 0;
}
