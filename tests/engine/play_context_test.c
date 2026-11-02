/* I994a: PLAY_CONTEXT. Driven through the engine command entry (send + pump); stats context_plays / context_rows are the
 * sentinel that the verb ran. Real modules (argv[1] = gen_testmod) so a started song really becomes the playing one.
 * Covers: the clicked row plays and the rows after it follow (flag CONTEXT), hand-added songs go before the context, a new
 * PLAY_CONTEXT replaces only the old context rows, one QUEUE_CHANGED and one UNDO per verb, a stale version and a start past
 * the end change nothing, the name is stored and cut on a UTF-8 boundary, QUEUE_MOVE flips the flag, QUEUE_CLEAR clears the
 * name, the context is cut at the queue capacity (8 here) and the oldest played context rows make room. */
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

#define NSONGS 10u
#define CAP_BIG 32u /* the undo dead table holds one entry per queue slot: big enough for the whole first part */
#define CAP_SMALL 8u

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
    sel_engine_pump_reads(eng, sink, NULL, 60000u);
}

static sel_engine_stats stats(sel_engine* e) {
    sel_engine_stats s;
    sel_engine_stats_get(e, &s);
    return s;
}

static uint32_t qcount(sel_engine* e) { return sel_engine_state(e)->queue_count; }
static uint32_t vversion(sel_engine* e) { return sel_engine_state(e)->view->version; }
static int is_ctx(sel_engine* e, uint32_t i) { return (sel_engine_state(e)->queue[i].flags & SEL_QUEUE_FLAG_CONTEXT) != 0u; }

static const char* qpath(sel_engine* e, uint32_t i) {
    const sel_state* st = sel_engine_state(e);
    return st->queue_text + st->queue[i].path_offset;
}

static const char* playing_path(sel_engine* e) {
    const sel_state* st = sel_engine_state(e);
    uint32_t i;
    for (i = 0; i < st->queue_count; i++) {
        if (st->queue[i].id == st->queue_playing) return st->queue_text + st->queue[i].path_offset;
    }
    return "";
}

/* The queue as "a1c h0 a2c": a = playlist song, h = hand-added song, then its index; "c" marks a context row. */
static void shape(sel_engine* e, char (*f)[320], char (*h)[320], char* out, size_t cap) {
    uint32_t i, k;
    size_t n = 0;
    out[0] = '\0';
    for (i = 0; i < qcount(e); i++) {
        const char* p = qpath(e, i);
        char tag = '?';
        unsigned idx = 0;
        for (k = 0; k < NSONGS; k++) {
            if (strcmp(p, f[k]) == 0) {
                tag = 'a';
                idx = k;
            }
            if (k < 2u && strcmp(p, h[k]) == 0) {
                tag = 'h';
                idx = k;
            }
        }
        n += (size_t)snprintf(out + n, cap - n, "%s%c%u%s", i ? " " : "", tag, idx, is_ctx(e, i) ? "c" : "");
    }
}

#define SHAPE_IS(want)                                                         \
    do {                                                                       \
        shape(e, f, h, sh, sizeof sh);                                         \
        if (strcmp(sh, want) != 0) printf("shape '%s' want '%s'\n", sh, want); \
        CHECK(strcmp(sh, want) == 0);                                          \
    } while (0)

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_playctx_XXXXXX";
    char db[256], line[600], cmd[700], sh[256], f[NSONGS][320], h[2][320];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    uint32_t i, plays, rows;
    if (argc < 2 || mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.queue_capacity = CAP_BIG;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    for (i = 0; i < NSONGS + 2u; i++) {
        char* p = i < NSONGS ? f[i] : h[i - NSONGS];
        snprintf(p, 320, "%s/s%u.mod", dir, (unsigned)i);
        snprintf(cmd, sizeof cmd, "\"%s\" --loop \"%s\"", argv[1], p);
        CHECK(system(cmd) == 0);
    }

    /* The playlist the context is played from: NSONGS songs, more than the queue holds. */
    run(e, "PLAYLIST_CREATE alpha");
    for (i = 0; i < NSONGS; i++) {
        snprintf(line, sizeof line, "PLAYLIST_ADD_SONG 1 \"%s\"", f[i]);
        run(e, line);
    }
    run(e, "VIEW_SOURCE folders");
    CHECK(qcount(e) == 0u);

    /* Start at row 1 of 10: rows 1..9 follow. */
    g_qchanged = 0;
    snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 1 \"Alpha\"", (unsigned)vversion(e));
    run(e, line);
    CHECK(stats(e).context_plays == 1u && stats(e).context_rows == 9u);
    SHAPE_IS("a1c a2c a3c a4c a5c a6c a7c a8c a9c");
    CHECK(strcmp(playing_path(e), f[1]) == 0);
    CHECK(strcmp(sel_engine_state(e)->ctx_name, "Alpha") == 0);
    CHECK(g_qchanged == 1u);

    /* A second context replaces the rows after the playing one; the played row stays as history. */
    snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 5 \"Alpha\"", (unsigned)vversion(e));
    run(e, line);
    SHAPE_IS("a1c a5c a6c a7c a8c a9c");
    CHECK(strcmp(playing_path(e), f[5]) == 0);

    /* Hand-added songs go before the rest of the context. */
    snprintf(line, sizeof line, "QUEUE_ADD \"%s\"", h[0]);
    run(e, line);
    snprintf(line, sizeof line, "QUEUE_ADD \"%s\"", h[1]);
    run(e, line);
    SHAPE_IS("a1c a5c h0 h1 a6c a7c a8c a9c");
    CHECK(!is_ctx(e, 2) && !is_ctx(e, 3));

    /* A new context replaces only the old context rows after the playing one; hand-added rows stay, the clicked row plays
     * first and the rest of the list follows the hand-added rows. */
    plays = stats(e).context_plays;
    rows = stats(e).context_rows;
    g_qchanged = 0;
    snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 2 \"Beta\"", (unsigned)vversion(e));
    run(e, line);
    SHAPE_IS("a1c a5c a2c h0 h1 a3c a4c a5c a6c a7c a8c a9c");
    CHECK(strcmp(playing_path(e), f[2]) == 0 && g_qchanged == 1u);
    CHECK(stats(e).context_plays == plays + 1u && stats(e).context_rows == rows + 8u);
    CHECK(strcmp(sel_engine_state(e)->ctx_name, "Beta") == 0);

    /* One UNDO restores the previous queue, flags included. */
    run(e, "UNDO");
    SHAPE_IS("a1c a5c h0 h1 a6c a7c a8c a9c");

    /* A stale version changes nothing. */
    plays = stats(e).context_plays;
    g_qchanged = 0;
    snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 0 \"Gamma\"", (unsigned)vversion(e) - 1u);
    run(e, line);
    SHAPE_IS("a1c a5c h0 h1 a6c a7c a8c a9c");
    CHECK(stats(e).context_plays == plays && g_qchanged == 0u && stats(e).songs_stale >= 1u);
    CHECK(strcmp(sel_engine_state(e)->ctx_name, "Beta") == 0);

    /* A start past the end is a range error and changes nothing. */
    snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 99 \"Gamma\"", (unsigned)vversion(e));
    run(e, line);
    SHAPE_IS("a1c a5c h0 h1 a6c a7c a8c a9c");
    CHECK(stats(e).context_plays == plays);

    /* QUEUE_CLEAR drops the name. */
    run(e, "QUEUE_CLEAR");
    CHECK(sel_engine_state(e)->ctx_name[0] == '\0' && qcount(e) == 0u);

    /* QUEUE_MOVE: a context row dragged into the hand-added block stops being context; a hand-added row moved below the
     * context rows becomes one. */
    snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 5 \"Alpha\"", (unsigned)vversion(e));
    run(e, line);
    snprintf(line, sizeof line, "QUEUE_ADD \"%s\"", h[0]);
    run(e, line);
    snprintf(line, sizeof line, "QUEUE_ADD \"%s\"", h[1]);
    run(e, line);
    SHAPE_IS("a5c h0 h1 a6c a7c a8c a9c");
    {
        const sel_state* st = sel_engine_state(e);
        uint32_t ctx_id = st->queue[5].id, hand_id = st->queue[1].id, last_id = st->queue[6].id;
        snprintf(line, sizeof line, "QUEUE_MOVE %u %u", (unsigned)ctx_id, (unsigned)hand_id);
        run(e, line);
        SHAPE_IS("a5c a8 h0 h1 a6c a7c a9c");
        snprintf(line, sizeof line, "QUEUE_MOVE %u %u", (unsigned)hand_id, (unsigned)last_id);
        run(e, line);
        SHAPE_IS("a5c a8 h1 a6c a7c a9c h0c");
    }

    /* A name of 100 two-byte letters is cut to 126 bytes, never inside a letter. */
    {
        char name[400];
        size_t n = 0;
        for (i = 0; i < 100u; i++) n += (size_t)snprintf(name + n, sizeof name - n, "\xc3\xa5");
        snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 0 \"%s\"", (unsigned)vversion(e), name);
        run(e, line);
        CHECK(strlen(sel_engine_state(e)->ctx_name) == 126u);
    }

    /* A queue of 8: rows 1..8 fit, row 9 is cut and reported once. */
    sel_engine_destroy(e);
    cfg.queue_capacity = CAP_SMALL;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    run(e, "VIEW_SOURCE folders");
    snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 1 \"Alpha\"", (unsigned)vversion(e));
    run(e, line);
    SHAPE_IS("a1c a2c a3c a4c a5c a6c a7c a8c");
    CHECK(stats(e).context_rows == 8u && strcmp(playing_path(e), f[1]) == 0);

    /* Played context rows make room: six clicks of five rows in a queue of 8. The clicked row always plays. */
    run(e, "QUEUE_CLEAR");
    for (i = 0; i < 6u; i++) {
        snprintf(line, sizeof line, "PLAY_CONTEXT playlist:1 %u 5 \"Alpha\"", (unsigned)vversion(e));
        run(e, line);
        CHECK(qcount(e) <= CAP_SMALL);
        CHECK(strcmp(playing_path(e), f[5]) == 0);
        CHECK(strcmp(qpath(e, qcount(e) - 1u), f[9]) == 0);
    }

    sel_engine_destroy(e);
    sel_library_close(lib);
    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", dir);
    (void)system(cmd);
    if (g_fail) {
        printf("play_context: %d failures\n", g_fail);
        return 1;
    }
    printf("play_context_ok\n");
    return 0;
}
