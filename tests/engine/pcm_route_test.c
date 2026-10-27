/* Regression (I532): playback was silent on the Amiga. The default output there is "paula", a voice-only output
 * (SEL_OUT_VOICES): its pump discards the engine's PCM. A RENDER-kind song (and any song the shell routes to the
 * mixer, since the shell wires no voice sink) therefore played to nowhere while the position kept moving.
 * Fix: before a mixer-routed song starts on a voice-only output, the engine asks the app (sel_engine_set_output_switch)
 * for a PCM sink: "ahi" when the machine's SEL_MACHINE_AUDIO list names it, else "paula14". A song that goes to the
 * voice sink directly stays on Paula.
 * Driven through the engine's command entry (QUEUE_ADD, PLAY, pump) with a registry the test controls. Sentinel: the
 * switch callback's call count and the id it received; state.output proves the engine took the new output.
 * Arguments: "ahi" (AUDIO paula,ahi), "paula" (AUDIO paula), "voice" (voice song with a sink stays on Paula). */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/protocol.h>
#include <selecta/replayer.h>
#include <selecta/tags.h>
#include <selecta/voices.h>
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
static uint32_t g_switches;
static char g_switch_id[32];

static int ends_with(const char* name, const char* ext) {
    size_t n = strlen(name), x = strlen(ext);
    return n >= x && strcmp(name + n - x, ext) == 0;
}

static int rnd_probe(const uint8_t* data, size_t size, const char* name) {
    (void)data;
    (void)size;
    return name != NULL && ends_with(name, ".rnd") ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static int vox_probe(const uint8_t* data, size_t size, const char* name) {
    (void)data;
    (void)size;
    return name != NULL && ends_with(name, ".vox") ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* rnd_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    (void)src;
    (void)host;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->voice_count = 0;
    return &g_state;
}

static void* vox_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    rnd_open(src, host, info);
    info->voice_count = 4;
    info->timebase_hz = 1000;
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

static uint32_t rnd_render(void* state, int16_t* out, uint32_t frames) {
    (void)state;
    memset(out, 0, (size_t)frames * 2u * sizeof *out);
    return frames;
}

static void vox_tick(void* state, sel_tick_result* out) {
    (void)state;
    out->length = 20;
    out->flags = 0;
}

static sel_tag g_rnd_hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_RENDER},
                                {SEL_RP_EXTENSIONS, (uintptr_t) "rnd"},
                                {SEL_RP_PROBE, SEL_TAG_FN(rnd_probe)},
                                {SEL_RP_OPEN, SEL_TAG_FN(rnd_open)},
                                {SEL_RP_CLOSE, SEL_TAG_FN(fake_close)},
                                {SEL_RP_START, SEL_TAG_FN(fake_start)},
                                {SEL_RP_RENDER, SEL_TAG_FN(rnd_render)},
                                {SEL_TAG_END, 0}};
static sel_tag g_vox_hooks[] = {{SEL_RP_KIND, SEL_RP_KIND_VOICE},
                                {SEL_RP_EXTENSIONS, (uintptr_t) "vox"},
                                {SEL_RP_PROBE, SEL_TAG_FN(vox_probe)},
                                {SEL_RP_OPEN, SEL_TAG_FN(vox_open)},
                                {SEL_RP_CLOSE, SEL_TAG_FN(fake_close)},
                                {SEL_RP_START, SEL_TAG_FN(fake_start)},
                                {SEL_RP_TICK, SEL_TAG_FN(vox_tick)},
                                {SEL_TAG_END, 0}};
static int g_voices_marker;
static sel_tag g_paula_hooks[] = {{SEL_OUT_VOICES, (uintptr_t)&g_voices_marker}, {SEL_OUT_VOICE_MAX, 4}, {SEL_TAG_END, 0}};
static sel_tag g_pcm_hooks[] = {{SEL_TAG_END, 0}}; /* a PCM sink: no SEL_OUT_VOICES */

static sel_tag g_desc[][4] = {
    {{SEL_PLUGIN_ID, (uintptr_t) "fake_rnd"},
     {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
     {SEL_PLUGIN_TAGS, (uintptr_t)g_rnd_hooks},
     {SEL_TAG_END, 0}},
    {{SEL_PLUGIN_ID, (uintptr_t) "fake_vox"},
     {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER},
     {SEL_PLUGIN_TAGS, (uintptr_t)g_vox_hooks},
     {SEL_TAG_END, 0}},
    {{SEL_PLUGIN_ID, (uintptr_t) "paula"},
     {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT},
     {SEL_PLUGIN_TAGS, (uintptr_t)g_paula_hooks},
     {SEL_TAG_END, 0}},
    {{SEL_PLUGIN_ID, (uintptr_t) "ahi"},
     {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT},
     {SEL_PLUGIN_TAGS, (uintptr_t)g_pcm_hooks},
     {SEL_TAG_END, 0}},
    {{SEL_PLUGIN_ID, (uintptr_t) "paula14"},
     {SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_OUTPUT},
     {SEL_PLUGIN_TAGS, (uintptr_t)g_pcm_hooks},
     {SEL_TAG_END, 0}},
};

static uint32_t reg_count(void) {
    return (uint32_t)(sizeof g_desc / sizeof g_desc[0]);
}

static const sel_tag* reg_get(uint32_t i) {
    return i < reg_count() ? g_desc[i] : NULL;
}

static int switch_cb(void* ctx, const char* id) {
    (void)ctx;
    g_switches++;
    snprintf(g_switch_id, sizeof g_switch_id, "%s", id);
    return 1;
}

static sel_engine* eng;

static void run(const char* line) {
    sel_message m;
    char pool[1536];
    if (sel_codec_decode(line, SEL_MSG_CMD, &m, pool, sizeof pool) != SEL_CODEC_OK) {
        CHECK(0);
        return;
    }
    sel_engine_send(eng, &m);
    sel_engine_pump(eng, NULL, NULL);
}

static void touch(const char* dir, const char* name) {
    char p[1024];
    FILE* f;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (f != NULL) {
        fputs("song", f);
        fclose(f);
    }
}

/* Plays `name` on an engine whose open output is Paula and whose machine lists `audio`. */
static void play(const char* dir, const char* name, const char* audio, int with_voice_sink) {
    static sel_tag machine[2];
    static sel_voices sink;
    sel_engine_config cfg;
    char line[1200];
    machine[0].tag = SEL_MACHINE_AUDIO;
    machine[0].value = (uintptr_t)audio;
    machine[1].tag = SEL_TAG_END;
    machine[1].value = 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    cfg.machine = machine;
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    sink.size = sizeof sink;
    sel_engine_set_output(eng, g_paula_hooks);
    if (with_voice_sink) {
        sel_engine_set_voice_sink(eng, &sink);
    }
    sel_engine_set_output_switch(eng, switch_cb, NULL);
    g_switches = 0;
    g_switch_id[0] = '\0';
    run("OPTION fade_ms 0");
    snprintf(line, sizeof line, "QUEUE_ADD %s/%s", dir, name);
    run(line);
    run("PLAY");
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "ahi";
    char dir[] = "/tmp/selecta_pcm_route_XXXXXX";
    char cmd[128];
    CHECK(mkdtemp(dir) != NULL);
    touch(dir, "a.rnd");
    touch(dir, "b.vox");
    if (strcmp(mode, "ahi") == 0) {
        play(dir, "a.rnd", "paula,ahi", 0);
        CHECK(g_switches == 1u); /* sentinel: the swap ran */
        CHECK(strcmp(g_switch_id, "ahi") == 0);
        CHECK(strcmp(sel_engine_state(eng)->output, "ahi") == 0);
    } else if (strcmp(mode, "paula") == 0) {
        play(dir, "a.rnd", "paula", 0);
        CHECK(g_switches == 1u);
        CHECK(strcmp(g_switch_id, "paula14") == 0);
        CHECK(strcmp(sel_engine_state(eng)->output, "paula14") == 0);
    } else {
        play(dir, "b.vox", "paula,ahi", 1);
        CHECK(sel_engine_routed_direct(eng) == 1u); /* sentinel: the voice song really went to the sink */
        CHECK(g_switches == 0u);
    }
    sel_engine_destroy(eng);
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) {
        g_fail++;
    }
    if (g_fail == 0) {
        puts("pcm_route: ok");
    }
    return g_fail != 0;
}
