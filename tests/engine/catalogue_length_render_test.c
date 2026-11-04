/* I434: a RENDER-kind replayer (the libsidplayfp plugin) gets its length from the catalogue `lengths_ms` sticker the HVSC
 * sync writes. Driven through QUEUE_ADD, PLAY, pump and read from sel_engine_state()->song.duration_ms. Sentinel:
 * sel_engine_catalogue_lengths counts the lookup. Fails before song_length_ms consults the catalogue (only the hardware
 * path did). */
#include "db/db.h"
#include "db/db_internal.h"
#include "song_key.h"

#include <selecta/engine.h>
#include <selecta/machine.h>
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
    sel_engine_pump_reads(eng, NULL, NULL, 2000u); /* a song start waits for its posted library read */
}

extern uint32_t sel_engine_catalogue_lengths;

int main(void) {
    char dir[] = "/tmp/selecta_cat_len_XXXXXX";
    char song[512], db[512], key[1400], line[1200], cmd[1200];
    sel_engine_config cfg;
    sel_engine* eng;
    sel_library* lib;
    int16_t buf[441u * 2u];
    uint32_t i, cat0;
    FILE* f;
    if (mkdtemp(dir) == NULL) return 2;
    snprintf(song, sizeof song, "%s/known.sdb", dir);
    snprintf(db, sizeof db, "%s/library.db", dir);
    f = fopen(song, "wb");
    fputs("known song bytes", f);
    fclose(f);
    lib = sel_library_open(sel_host_default(), sel_machine_probe(sel_host_default()), 0, db);
    CHECK(lib != NULL);
    if (lib == NULL) return 2;
    CHECK(sel_song_key_of(song, key, sizeof key));
    CHECK(sel_library_sticker_set(lib, key, "lengths_ms", "123456"));
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    cfg.library = lib;
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    if (eng == NULL) return 2;
    snprintf(line, sizeof line, "QUEUE_ADD %s", song);
    run(eng, line);
    cat0 = sel_engine_catalogue_lengths;
    run(eng, "PLAY");
    for (i = 0; i < 5; i++) {
        sel_engine_render(eng, buf, 441u);
        sel_engine_pump(eng, NULL, NULL);
    }
    CHECK(sel_engine_catalogue_lengths > cat0); /* sentinel: the engine looked the catalogue length up */
    CHECK(sel_engine_state(eng)->song.duration_ms == 123456u);
    sel_engine_destroy(eng);
    sel_library_close(lib);
    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", dir);
    (void)system(cmd);
    if (g_fail) {
        printf("catalogue_length_render: %d failures\n", g_fail);
        return 1;
    }
    printf("catalogue_length_render_ok\n");
    return 0;
}
