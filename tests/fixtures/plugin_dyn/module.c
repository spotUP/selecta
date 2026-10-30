/* Fixture module for the plugin loader (I427a, design 2026-10-30-plugin-loader-sdk D1-D4). One source, three
 * builds picked by compile definitions:
 *   default (DYN_ID "dyntone")  the good module: a RENDER replayer claiming *.seldyn files that plays a half second
 *                               stereo ramp (left (3 n mod 2000) - 1000, right the negative).
 *   DYN_ID "ym"                 the same descriptor under the id of a built-in plugin: the loader keeps the static one.
 *   DYN_API_MISMATCH            sel_plugin_entry answers NULL for every host API version.
 * Includes include/selecta only, like any SDK plugin. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

#ifndef DYN_ID
#define DYN_ID "dyntone"
#endif

#ifndef DYN_API_MISMATCH
typedef struct dyn_state {
    const sel_host* host;
    uint32_t total, done;
} dyn_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return size >= 6u && memcmp(data, "SELDYN", 6) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    dyn_state* s;
    if (src->data == NULL || rp_probe(src->data, src->size, "") != SEL_PROBE_YES) {
        return NULL;
    }
    s = (dyn_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
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
    dyn_state* s = (dyn_state*)state;
    if (s != NULL) {
        s->host->mem_free(s->host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    dyn_state* s = (dyn_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->total = rate / 2u;
    s->done = 0;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    dyn_state* s = (dyn_state*)state;
    uint32_t n = s->total - s->done < frames ? s->total - s->done : frames;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int16_t v = (int16_t)((int)(((s->done + i) * 3u) % 2000u) - 1000);
        out[i * 2u] = v;
        out[i * 2u + 1u] = (int16_t)-v;
    }
    s->done += n;
    return n;
}

static sel_tag g_hooks[10];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

#endif /* DYN_API_MISMATCH */

const sel_tag* sel_plugin_entry(uint32_t host_api) {
#ifdef DYN_API_MISMATCH
    (void)host_api;
    return NULL;
#else
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t)DYN_ID);
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "seldyn");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t)DYN_ID);
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
#endif
}
