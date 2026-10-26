/* I257e: the engine state's lyric_prev / lyric_next (the lines before and after the current one, shown by the Now Playing
 * panel and the text UI). Driven through the engine (QUEUE_ADD, PLAY, SEEK, pump) with one fake RENDER replayer and a
 * sidecar .lrc written next to the song. Cases: time before the first line, first line (no prev), a middle line, the last
 * line (no next), a song whose .lrc is empty and one with no .lrc (no lines, no neighbours).
 * The sentinel is lyric_lines: the lines the engine really loaded, so a case can not pass on an unloaded .lrc. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static int g_state;

static int fake_probe(const uint8_t* d, size_t s, const char* name) {
    size_t n = name != NULL ? strlen(name) : 0u;
    (void)d;
    (void)s;
    return n >= 5u && strcmp(name + n - 5u, ".fake") == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* fake_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    (void)src;
    (void)host;
    info->subsong_count = 1;
    info->default_subsong = 0;
    return &g_state;
}

static void fake_close(void* state) {
    (void)state;
}

static int fake_start(void* st, uint32_t sub, uint32_t rate, const sel_voices* v) {
    (void)st;
    (void)sub;
    (void)rate;
    (void)v;
    return 1;
}

static uint32_t fake_render(void* state, int16_t* out, uint32_t frames) {
    (void)state;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}

static sel_tag hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER},
                          {SEL_RP_PROBE, SEL_TAG_FN(fake_probe)},
                          {SEL_RP_OPEN, SEL_TAG_FN(fake_open)},
                          {SEL_RP_CLOSE, SEL_TAG_FN(fake_close)},
                          {SEL_RP_START, SEL_TAG_FN(fake_start)},
                          {SEL_RP_RENDER, SEL_TAG_FN(fake_render)},
                          {SEL_TAG_END, 0}};
static sel_tag desc[] = {{SEL_PLUGIN_ID, (uintptr_t) "fakelyric"},
                         {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                         {SEL_PLUGIN_TAGS, (uintptr_t)hooks},
                         {SEL_TAG_END, 0}};

static uint32_t reg_count(void) {
    return 1u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0u ? desc : NULL;
}

static void send_line(sel_engine* eng, const char* line) {
    sel_message m;
    char pool[1536];
    CHECK(sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) == SEL_CODEC_OK);
    sel_engine_send(eng, &m);
}

static void pump(sel_engine* eng) {
    static int16_t buf[441 * 2];
    uint32_t t;
    for (t = 0; t < 5u; t++) {
        sel_engine_pump(eng, NULL, NULL);
        sel_engine_render(eng, buf, 441); /* the audio side: playback starts and the position moves in render */
    }
}

/* Plays dir/<name>.fake with `lrc` as dir/<name>.lrc (NULL: no file), seeks to `ms`, and returns the engine. */
static sel_engine* play_at(const char* dir, const char* name, const char* lrc, uint32_t ms) {
    sel_engine_config cfg;
    sel_engine* eng;
    char p[1024], line[1100];
    FILE* f;
    snprintf(p, sizeof p, "%s/%s.fake", dir, name);
    f = fopen(p, "wb");
    CHECK(f != NULL);
    fputs("song", f);
    fclose(f);
    snprintf(p, sizeof p, "%s/%s.lrc", dir, name);
    if (lrc != NULL) {
        f = fopen(p, "wb");
        CHECK(f != NULL);
        fputs(lrc, f);
        fclose(f);
    }
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    snprintf(line, sizeof line, "QUEUE_ADD %s/%s.fake", dir, name);
    send_line(eng, line);
    send_line(eng, "PLAY");
    pump(eng); /* SEEK needs the song open */
    snprintf(line, sizeof line, "SEEK %u", (unsigned)ms);
    send_line(eng, line);
    pump(eng);
    return eng;
}

static int is(const char* got, const char* want) {
    return got != NULL && strcmp(got, want) == 0;
}

int main(void) {
    char dir[] = "/tmp/selecta_lyric_nb_XXXXXX";
    static const char lrc[] = "[00:02.00]First\n[00:04.00]Second\n[00:06.00]Third\n";
    sel_engine* eng;
    const sel_state* st;
    CHECK(mkdtemp(dir) != NULL);

    eng = play_at(dir, "a", lrc, 500); /* before the first line */
    st = sel_engine_state(eng);
    CHECK(st->lyric_lines == 3u);
    CHECK(st->lyric_line == -1);
    CHECK(st->lyric_current == NULL && st->lyric_prev == NULL);
    CHECK(is(st->lyric_next, "First"));
    sel_engine_destroy(eng);

    eng = play_at(dir, "b", lrc, 2500); /* first line: no previous */
    st = sel_engine_state(eng);
    CHECK(st->lyric_lines == 3u && st->lyric_line == 0);
    CHECK(is(st->lyric_current, "First") && st->lyric_prev == NULL && is(st->lyric_next, "Second"));
    sel_engine_destroy(eng);

    eng = play_at(dir, "c", lrc, 4500); /* middle line: both neighbours */
    st = sel_engine_state(eng);
    CHECK(st->lyric_lines == 3u && st->lyric_line == 1);
    CHECK(is(st->lyric_current, "Second") && is(st->lyric_prev, "First") && is(st->lyric_next, "Third"));
    sel_engine_destroy(eng);

    eng = play_at(dir, "d", lrc, 6500); /* last line: no next */
    st = sel_engine_state(eng);
    CHECK(st->lyric_lines == 3u && st->lyric_line == 2);
    CHECK(is(st->lyric_current, "Third") && is(st->lyric_prev, "Second") && st->lyric_next == NULL);
    sel_engine_destroy(eng);

    eng = play_at(dir, "e", "", 4500); /* empty .lrc: no lines */
    st = sel_engine_state(eng);
    CHECK(st->lyric_lines == 0u && st->lyric_line == -1);
    CHECK(st->lyric_current == NULL && st->lyric_prev == NULL && st->lyric_next == NULL);
    sel_engine_destroy(eng);

    eng = play_at(dir, "f", NULL, 4500); /* no .lrc at all */
    st = sel_engine_state(eng);
    CHECK(st->lyric_lines == 0u && st->lyric_prev == NULL && st->lyric_next == NULL);
    sel_engine_destroy(eng);

    if (g_fail == 0) {
        printf("lyric_neighbours: ok\n");
    }
    return g_fail != 0;
}
