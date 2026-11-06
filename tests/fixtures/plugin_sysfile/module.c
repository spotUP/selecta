/* I710ga6: test-only loadable replayer "sysfile_probe". Claims *.sfp files and declares one synthetic system file,
 * "probe.bin" (label "Test system file"). The engine reports system_file_missing and never calls RP_OPEN until the file
 * is installed; then it renders a quiet sine for three seconds. The test supplies the identity at run time through
 * SELECTA_PROBE_SYSFILE_SHA (sha256 of the synthetic file the test generates) and SELECTA_PROBE_SYSFILE_URLS
 * (';'-separated). Built only with the tests (tests/fixtures/plugin_sysfile), never part of a release or the player pack.
 * Includes include/selecta only, like any SDK plugin. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

typedef struct probe_state {
    const sel_host* host;
    uint32_t total, done, rate;
} probe_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t n = name != NULL ? strlen(name) : 0u;
    (void)data;
    (void)size;
    return n >= 4u && strcmp(name + n - 4u, ".sfp") == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    probe_state* s;
    const char* path = host->config_get(host, "sysfile.probe.bin", NULL);
    (void)src;
    if (path == NULL || path[0] == '\0') {
        return NULL;
    }
    s = (probe_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    probe_state* s = (probe_state*)state;
    if (s != NULL) {
        s->host->mem_free(s->host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    probe_state* s = (probe_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    s->total = rate * 3u;
    s->done = 0;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    probe_state* s = (probe_state*)state;
    uint32_t n = s->total - s->done < frames ? s->total - s->done : frames;
    uint32_t i;
    for (i = 0; i < n; i++) {
        uint32_t ph = ((s->done + i) * 440u) % s->rate; /* triangle at 440 Hz */
        int v = (int)(ph * 4000u / s->rate) - 2000;
        out[i * 2u] = (int16_t)v;
        out[i * 2u + 1u] = (int16_t)v;
    }
    s->done += n;
    return n;
}

static sel_tag g_sysfile[6];
static sel_tag g_hooks[11];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* sel_plugin_entry(uint32_t host_api) {
    const char* sha = getenv("SELECTA_PROBE_SYSFILE_SHA");
    const char* urls = getenv("SELECTA_PROBE_SYSFILE_URLS");
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_sysfile[0], SEL_RP_SYSFILE_NAME, (uintptr_t) "probe.bin");
    set(&g_sysfile[1], SEL_RP_SYSFILE_LABEL, (uintptr_t) "Test system file");
    set(&g_sysfile[2], SEL_RP_SYSFILE_HINT, (uintptr_t) "A synthetic file made by the test.");
    set(&g_sysfile[3], SEL_RP_SYSFILE_SHA256, (uintptr_t)(sha != NULL ? sha : ""));
    set(&g_sysfile[4], SEL_RP_SYSFILE_URLS, (uintptr_t)(urls != NULL ? urls : ""));
    set(&g_sysfile[5], SEL_TAG_END, 0);

    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sysfile_probe");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sfp");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SYSFILE, (uintptr_t)g_sysfile);
    set(&g_hooks[10], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sysfile_probe");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
