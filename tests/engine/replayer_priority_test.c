/* Regression (I293): find_replayer returned the first replayer that probed YES in registration order and ignored
 * SEL_RP_PRIORITY, so a generic replayer registered early (nuade) shadowed a native one declaring a higher priority
 * (kplayer, 10). The highest priority wins among the YES answers, else among the UNSURE ones; ties keep registration
 * order (as sel_registry_format_owner does).
 * Driven through the engine (QUEUE_ADD, PLAY, pump) with a registry the test controls: seven fake RENDER replayers
 * whose RP_START counts which one the engine opened. The start counter of the winner is the sentinel. */
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
static uint32_t g_starts[7];

static int ends_with(const char* name, const char* ext) {
    size_t n = strlen(name), x = strlen(ext);
    return n >= x && strcmp(name + n - x, ext) == 0;
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

static uint32_t fake_render(void* state, int16_t* out, uint32_t frames) {
    (void)state;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}

/* One fake: index, priority, extension it answers YES to, extension it answers UNSURE to. */
#define FAKE(n, pri, yes_ext, unsure_ext)                                            \
    static int probe##n(const uint8_t* d, size_t s, const char* name) {              \
        (void)d;                                                                     \
        (void)s;                                                                     \
        if (name != NULL && ends_with(name, yes_ext)) {                              \
            return SEL_PROBE_YES;                                                    \
        }                                                                            \
        if (name != NULL && ends_with(name, unsure_ext)) {                           \
            return SEL_PROBE_UNSURE;                                                 \
        }                                                                            \
        return SEL_PROBE_NO;                                                         \
    }                                                                                \
    static int start##n(void* st, uint32_t sub, uint32_t rate, const sel_voices* v) { \
        (void)st;                                                                    \
        (void)sub;                                                                   \
        (void)rate;                                                                  \
        (void)v;                                                                     \
        g_starts[n]++;                                                               \
        return 1;                                                                    \
    }                                                                                \
    static sel_tag hooks##n[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER},                  \
                                 {SEL_RP_PRIORITY, (pri)},                           \
                                 {SEL_RP_PROBE, SEL_TAG_FN(probe##n)},               \
                                 {SEL_RP_OPEN, SEL_TAG_FN(fake_open)},               \
                                 {SEL_RP_CLOSE, SEL_TAG_FN(fake_close)},             \
                                 {SEL_RP_START, SEL_TAG_FN(start##n)},               \
                                 {SEL_RP_RENDER, SEL_TAG_FN(fake_render)},           \
                                 {SEL_TAG_END, 0}};                                  \
    static sel_tag desc##n[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake" #n},              \
                                {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},         \
                                {SEL_PLUGIN_TAGS, (uintptr_t)hooks##n},              \
                                {SEL_TAG_END, 0}}

/* .pri: 0 (priority 0) and 1 (priority 10) both YES -> 1 wins although registered later.
 * .tie: 2 and 3 (both priority 5) both YES -> 2, the first registered.
 * .uns: 4 (priority 0) and 5 (priority 3) both UNSURE, 6 (priority 9) NO -> 5. */
FAKE(0, 0, ".pri", ".none");
FAKE(1, 10, ".pri", ".none");
FAKE(2, 5, ".tie", ".none");
FAKE(3, 5, ".tie", ".none");
FAKE(4, 0, ".none", ".uns");
FAKE(5, 3, ".none", ".uns");
FAKE(6, 9, ".none", ".none");

static sel_tag* const g_desc[7] = {desc0, desc1, desc2, desc3, desc4, desc5, desc6};

static uint32_t reg_count(void) {
    return 7u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i < 7u ? g_desc[i] : NULL;
}

/* Opens `name` in a fresh engine and returns the index of the fake that started, or -1. */
static int winner(const char* dir, const char* name) {
    sel_engine_config cfg;
    sel_engine* eng;
    sel_message m;
    char pool[1536], line[1200], p[1024];
    uint32_t i, t;
    int w = -1, hits = 0;
    FILE* f;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (f == NULL) {
        return -2;
    }
    fputs("song", f);
    fclose(f);
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    memset(g_starts, 0, sizeof g_starts);
    snprintf(line, sizeof line, "QUEUE_ADD %s", p);
    CHECK(sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) == SEL_CODEC_OK);
    sel_engine_send(eng, &m);
    CHECK(sel_codec_decode("PLAY", SEL_MSG_CMD, &m, pool, sizeof pool) == SEL_CODEC_OK);
    sel_engine_send(eng, &m);
    for (t = 0; t < 5u; t++) {
        sel_engine_pump(eng, NULL, NULL);
    }
    for (i = 0; i < 7u; i++) {
        if (g_starts[i] != 0u) {
            w = (int)i;
            hits++;
        }
    }
    CHECK(hits == 1); /* sentinel: exactly one replayer ran */
    sel_engine_destroy(eng);
    return w;
}

int main(void) {
    char dir[] = "/tmp/selecta_rp_priority_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    CHECK(winner(dir, "a.pri") == 1); /* later, higher priority wins over the earlier YES */
    CHECK(winner(dir, "a.tie") == 2); /* tie keeps registration order */
    CHECK(winner(dir, "a.uns") == 5); /* no YES: the highest-priority UNSURE */
    if (g_fail == 0) {
        printf("replayer_priority: ok\n");
    }
    return g_fail != 0;
}
