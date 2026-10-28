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

static const char* playing_path(sel_engine* e) {
    const sel_state* st = sel_engine_state(e);
    uint32_t i;
    for (i = 0; i < st->queue_count; i++) {
        if (st->queue[i].id == st->queue_playing) return st->queue_text + st->queue[i].path_offset;
    }
    return "";
}

/* Rows of the window that name a song (a Home or Search song row, not an album, composer, artist-less heading). */
static uint32_t home_song_rows(sel_engine* e) {
    const sel_view_state* v = sel_engine_state(e)->view;
    uint32_t i, n = 0;
    for (i = 0; i < v->row_n; i++) {
        if (v->rows[i].kind == SEL_ROW_HOME && v->rows[i].key && !v->rows[i].album && !v->rows[i].composer) n++;
    }
    return n;
}

static void mkmod(const char* tool, const char* path) {
    char cmd[700];
    snprintf(cmd, sizeof cmd, "\"%s\" --loop \"%s\"", tool, path);
    CHECK(system(cmd) == 0);
}

/* I632b2: the view list in every style and source, and SONGS_PLAY. Real modules (argv[1] = gen_testmod) so that a started
 * song really becomes the playing one. Layout x/a x/b y/c y/d under dir/real. */
static void test_views(sel_engine* e, sel_library* lib, const char* dir, const char* tool) {
    char real[300], px[300], py[300], f[4][320], line[400], tsv[2048], hist[320];
    sel_lib_job job;
    sel_engine_stats st;
    uint32_t i, before, runs, plid = 0, n;
    snprintf(real, sizeof real, "%s/real", dir);
    snprintf(px, sizeof px, "%s/x", real);
    snprintf(py, sizeof py, "%s/y", real);
    snprintf(line, sizeof line, "mkdir -p \"%s\" \"%s\"", px, py);
    CHECK(system(line) == 0);
    snprintf(f[0], sizeof f[0], "%s/a.mod", px);
    snprintf(f[1], sizeof f[1], "%s/b.mod", px);
    snprintf(f[2], sizeof f[2], "%s/c.mod", py);
    snprintf(f[3], sizeof f[3], "%s/d.mod", py);
    tsv[0] = '\0';
    for (i = 0; i < 4; i++) {
        mkmod(tool, f[i]);
        snprintf(tsv + strlen(tsv), sizeof tsv - strlen(tsv), "%s\tlocal\t%s\tsong %c\tzedartist\tmod\t1994\n", f[i], f[i], (char)('a' + i));
    }
    memset(&job, 0, sizeof job);
    job.text = tsv;
    CHECK(sel_library_run(lib, SEL_LIB_REQ_IMPORT, &job) && job.count == 4u);

    /* SONGS_PLAY on the queue plays in place: no song is added, the first selected row starts. */
    run(e, "QUEUE_CLEAR");
    run(e, "PLAYLIST_CREATE beta"); /* playlist 2, empty */
    for (i = 0; i < 4; i++) {
        snprintf(line, sizeof line, "QUEUE_ADD \"%s\"", f[i]);
        run(e, line);
    }
    snprintf(line, sizeof line, "SONGS_PLAY queue %u \"2-3\"", (unsigned)qversion(e));
    runs = stats(e).songs_runs;
    run(e, line);
    CHECK(stats(e).songs_runs == runs + 1u && stats(e).songs_done == 2u && qcount(e) == 4u);
    CHECK(strcmp(playing_path(e), f[2]) == 0);
    /* The same on a list that is not the queue: the songs join after the playing one and the first starts. */
    snprintf(line, sizeof line, "SONGS_PLAYLIST_ADD 2 queue %u \"0-3\"", (unsigned)qversion(e));
    run(e, line);
    CHECK(stats(e).songs_done == 4u);
    run(e, "QUEUE_CLEAR");
    snprintf(line, sizeof line, "QUEUE_ADD \"%s\"", f[0]);
    run(e, line);
    run(e, "VIEW_SOURCE folders");
    snprintf(line, sizeof line, "SONGS_PLAY playlist:2 %u \"1,3\"", (unsigned)vversion(e));
    run(e, line);
    CHECK(stats(e).songs_done == 2u && qcount(e) == 3u);
    CHECK(strcmp(playing_path(e), f[1]) == 0 && strcmp(qpath(e, 0), f[1]) == 0 && strcmp(qpath(e, 1), f[3]) == 0 && strcmp(qpath(e, 2), f[0]) == 0);

    /* Library view (Home): heading rows are skipped and counted, song rows are handled. */
    run(e, "QUEUE_CLEAR");
    for (i = 0; i < 3; i++) CHECK(sel_library_history_add(lib, f[i], 1000 + (long)i, 0));
    snprintf(hist, sizeof hist, "%s", f[0]);
    run(e, "VIEW_WINDOW 60");
    run(e, "VIEW_SOURCE home");
    n = sel_engine_state(e)->view->row_count;
    CHECK(n > 0u && sel_engine_state(e)->view->row_n == n);
    i = home_song_rows(e);
    CHECK(i >= 3u && i < n);
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD view %u \"0-%u\"", (unsigned)vversion(e), (unsigned)(n - 1u));
    run(e, line);
    st = stats(e);
    CHECK(st.songs_done == i && st.songs_skipped == n - i && qcount(e) == i);

    /* Search results: the songs and the artist row. Play starts the first song and queues them all. */
    run(e, "QUEUE_CLEAR");
    run(e, "LIBRARY_SEARCH zedartist");
    run(e, "VIEW_SOURCE search");
    n = sel_engine_state(e)->view->row_count;
    CHECK(n > 0u && sel_engine_state(e)->view->row_n == n);
    i = home_song_rows(e);
    CHECK(i >= 4u && i < n);
    snprintf(line, sizeof line, "SONGS_PLAY view %u \"0-%u\"", (unsigned)vversion(e), (unsigned)(n - 1u));
    run(e, line);
    st = stats(e);
    CHECK(st.songs_done == i && st.songs_skipped == n - i && qcount(e) == i && strcmp(playing_path(e), qpath(e, 0)) == 0);

    /* Query source in the tree: node rows are skipped, the songs under the open nodes are handled in tree order. */
    run(e, "QUEUE_CLEAR");
    run(e, "LIBRARY_SEARCH zedartist");
    run(e, "VIEW_SOURCE query");
    run(e, "VIEW_GROUP folder");
    CHECK(sel_engine_state(e)->view->style == SEL_VIEW_STYLE_TREE && sel_engine_state(e)->view->row_count == 2u);
    run(e, "VIEW_EXPAND 1");
    run(e, "VIEW_EXPAND 2");
    CHECK(sel_engine_state(e)->view->row_count == 6u); /* x a b y c d */
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD view %u \"1-3\"", (unsigned)vversion(e)); /* a b y: two songs, one node */
    run(e, line);
    st = stats(e);
    CHECK(st.songs_done == 2u && st.songs_skipped == 1u && qcount(e) == 2u && strcmp(qpath(e, 0), f[0]) == 0 && strcmp(qpath(e, 1), f[1]) == 0);
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD view %u \"0-5\"", (unsigned)vversion(e));
    run(e, line);
    st = stats(e);
    CHECK(st.songs_done == 4u && st.songs_skipped == 2u && qcount(e) == 6u && strcmp(qpath(e, 5), f[3]) == 0);
    run(e, "VIEW_COLLAPSE 1");
    run(e, "VIEW_COLLAPSE 2");
    before = qcount(e);
    snprintf(line, sizeof line, "SONGS_PLAY_NEXT view %u \"0-1\"", (unsigned)vversion(e)); /* two nodes: nothing to do, no error */
    runs = stats(e).songs_runs;
    run(e, line);
    st = stats(e);
    CHECK(st.songs_runs == runs + 1u && st.songs_done == 0u && st.songs_skipped == 2u && qcount(e) == before);

    /* Drilldown: the node level has only nodes, the song level only songs. */
    run(e, "VIEW_STYLE drilldown");
    CHECK(sel_engine_state(e)->view->row_count == 2u);
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD view %u \"0-1\"", (unsigned)vversion(e));
    run(e, line);
    st = stats(e);
    CHECK(st.songs_done == 0u && st.songs_skipped == 2u && qcount(e) == before);
    snprintf(line, sizeof line, "VIEW_DRILL 0 %s", py);
    run(e, line);
    CHECK(sel_engine_state(e)->view->row_count == 2u);
    snprintf(line, sizeof line, "SONGS_QUEUE_ADD view %u \"0-1\"", (unsigned)vversion(e));
    run(e, line);
    st = stats(e);
    CHECK(st.songs_done == 2u && st.songs_skipped == 0u && qcount(e) == before + 2u && strcmp(qpath(e, before), f[2]) == 0);

    /* Folders source: folder and playlist rows only, all skipped and counted. */
    CHECK(sel_library_folder_add(lib, "mixed", 0u, &plid) == 0);
    run(e, "VIEW_SOURCE folders");
    n = sel_engine_state(e)->view->row_count;
    CHECK(n >= 3u);
    before = qcount(e);
    snprintf(line, sizeof line, "SONGS_PLAY view %u \"0-%u\"", (unsigned)vversion(e), (unsigned)(n - 1u));
    runs = stats(e).songs_runs;
    run(e, line);
    st = stats(e);
    CHECK(st.songs_runs == runs + 1u && st.songs_done == 0u && st.songs_skipped == n && qcount(e) == before);
}

int main(int argc, char** argv) {
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

    if (argc > 1) test_views(e, lib, dir, argv[1]);
    CHECK(argc > 1);

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
