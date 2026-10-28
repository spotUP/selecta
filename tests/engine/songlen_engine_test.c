/* I605 reachability: a song whose replayer publishes no SEL_META_DURATION_MS gets its length from the song length
 * database (<config dir>/songlengths.bin), keyed by the UADE content key of the file. Driven through the engine's
 * public entry points (QUEUE_ADD, PLAY, pump) and read from sel_engine_state(), the struct STATUS prints
 * duration_ms from. Sentinel: the value is the database's, which nothing else can produce. A song whose key the
 * database lacks, and an engine without the file, stay at 0. Fails before the engine consults the database. */
#include "songlen.h"
#include "xxh32.h"

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

static int probe(const uint8_t* data, size_t size, const char* name) {
    size_t n = name != NULL ? strlen(name) : 0;
    (void)data;
    (void)size;
    return n >= 4 && strcmp(name + n - 4, ".sdb") == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
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

static int fake_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    (void)state;
    (void)subsong;
    (void)rate;
    (void)voices;
    return 1;
}

static uint32_t fake_render(void* state, int16_t* out, uint32_t frames) {
    (void)state;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}

static sel_tag g_hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER},
                            {SEL_RP_EXTENSIONS, (uintptr_t) "sdb"},
                            {SEL_RP_PROBE, SEL_TAG_FN(probe)},
                            {SEL_RP_OPEN, SEL_TAG_FN(fake_open)},
                            {SEL_RP_CLOSE, SEL_TAG_FN(fake_close)},
                            {SEL_RP_START, SEL_TAG_FN(fake_start)},
                            {SEL_RP_RENDER, SEL_TAG_FN(fake_render)},
                            {SEL_TAG_END, 0}};
static sel_tag g_desc[] = {{SEL_PLUGIN_ID, (uintptr_t) "fake_nolen"},
                           {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
                           {SEL_PLUGIN_TAGS, (uintptr_t)g_hooks},
                           {SEL_TAG_END, 0}};

static uint32_t reg_count(void) {
    return 1u;
}

static const sel_tag* reg_get(uint32_t i) {
    return i == 0u ? g_desc : NULL;
}

static void run(sel_engine* eng, const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, NULL, NULL);
}

/* Plays `song` on a fresh engine with config dir `cfgdir`; returns the state's duration_ms. */
static uint32_t duration_of(const char* cfgdir, const char* song) {
    sel_engine_config cfg;
    sel_engine* eng;
    char line[1200];
    int16_t buf[441u * 2u];
    uint32_t ms, i;
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    cfg.config_dir = cfgdir;
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    if (eng == NULL) return 0;
    snprintf(line, sizeof line, "QUEUE_ADD %s", song);
    run(eng, line);
    run(eng, "PLAY");
    for (i = 0; i < 5; i++) {
        sel_engine_render(eng, buf, 441u);
        sel_engine_pump(eng, NULL, NULL);
    }
    CHECK(sel_engine_state(eng)->song.path != NULL);
    ms = sel_engine_state(eng)->song.duration_ms;
    sel_engine_destroy(eng);
    return ms;
}

int main(void) {
    char dir[] = "/tmp/selecta_songlen_engine_XXXXXX";
    char song[512], other[512], tsv[512], bin[512], key[SEL_UADE_KEY_CHARS + 1u], cmd[1200];
    FILE* f;
    if (mkdtemp(dir) == NULL) return 2;
    snprintf(song, sizeof song, "%s/known.sdb", dir);
    snprintf(other, sizeof other, "%s/unknown.sdb", dir);
    snprintf(tsv, sizeof tsv, "%s/in.tsv", dir);
    snprintf(bin, sizeof bin, "%s/%s", dir, SEL_SONGLEN_NAME);
    f = fopen(song, "wb");
    fputs("known song bytes", f);
    fclose(f);
    f = fopen(other, "wb");
    fputs("a different file", f);
    fclose(f);
    CHECK(sel_uade_key_file(song, key));
    f = fopen(tsv, "wb");
    fprintf(f, "%s\t0\t187654,p\n", key);
    fclose(f);

    CHECK(duration_of(dir, song) == 0u); /* no songlengths.bin yet: unknown */
    CHECK(sel_songlen_bake(tsv, bin, NULL) == 1);
    CHECK(duration_of(dir, song) == 187654u); /* the database value reaches STATUS */
    CHECK(duration_of(dir, other) == 0u);     /* a key the database lacks stays unknown */

    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", dir);
    (void)system(cmd);
    if (g_fail) {
        printf("songlen_engine: %d failures\n", g_fail);
        return 1;
    }
    printf("songlen_engine_ok\n");
    return 0;
}
