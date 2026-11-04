/* Example Selecta replayer: plays a sine tone described by a tiny text file "SINE <hz> <ms>" (extension .sine).
 * Includes only <selecta/...> and libc; it does not link selecta_core. Hooks follow include/selecta/replayer.h,
 * the descriptor layout follows include/selecta/plugin.h and tags.h. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

typedef struct sine_state {
    const sel_host* host;
    uint32_t hz, ms, rate;
    uint32_t phase; /* 32-bit phase accumulator */
    uint64_t played, limit;
} sine_state;

static int parse(const uint8_t* data, size_t size, uint32_t* hz, uint32_t* ms) {
    char text[32];
    char* end;
    unsigned long a, b;
    if (size < 8u || memcmp(data, "SINE ", 5) != 0) {
        return 0;
    }
    if (size >= sizeof text) {
        size = sizeof text - 1u;
    }
    memcpy(text, data, size);
    text[size] = '\0';
    a = strtoul(text + 5, &end, 10);
    b = strtoul(end, &end, 10);
    if (a < 20ul || a > 20000ul || b == 0ul || b > 600000ul) {
        return 0;
    }
    *hz = (uint32_t)a;
    *ms = (uint32_t)b;
    return 1;
}

/* sel_rp_probe_fn */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    uint32_t hz, ms;
    (void)name;
    return parse(data, size, &hz, &ms) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

/* sel_rp_open_fn */
static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    sine_state* s;
    uint32_t hz, ms;
    if (!parse(src->data, src->size, &hz, &ms)) {
        return NULL;
    }
    s = (sine_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->hz = hz;
    s->ms = ms;
    info->voice_count = 1;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 1000;
    return s;
}

/* sel_rp_close_fn */
static void rp_close(void* state) {
    sine_state* s = (sine_state*)state;
    if (s != NULL) {
        s->host->mem_free(s->host, s);
    }
}

/* sel_rp_start_fn; a RENDER replayer gets voices = NULL */
static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    sine_state* s = (sine_state*)state;
    (void)voices;
    if (subsong != 0u || rate == 0u) {
        return 0;
    }
    s->rate = rate;
    s->phase = 0;
    s->played = 0;
    s->limit = (uint64_t)s->ms * rate / 1000u;
    return 1;
}

/* Integer parabolic sine at half amplitude: no float and no libm. */
static int16_t sine_sample(uint32_t phase) {
    uint32_t p = phase >> 16;
    uint32_t h = p & 0x7FFFu;
    int32_t y = (int32_t)((h * (32768u - h)) >> 14); /* 0..16384 */
    return (int16_t)((p & 0x8000u) != 0u ? -y : y);
}

/* sel_rp_render_fn: interleaved stereo int16, fewer frames than asked only at song end */
static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    sine_state* s = (sine_state*)state;
    uint32_t i;
    uint32_t step = (uint32_t)(((uint64_t)s->hz << 32) / s->rate);
    uint64_t left = s->limit > s->played ? s->limit - s->played : 0u;
    if (frames > left) {
        frames = (uint32_t)left;
    }
    for (i = 0; i < frames; i++) {
        int16_t v = sine_sample(s->phase);
        out[2u * i] = v;
        out[2u * i + 1u] = v;
        s->phase += step;
    }
    s->played += frames;
    return frames;
}

static sel_tag g_hooks[10];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

/* Descriptor: PLUGIN_KIND, PLUGIN_ID, PLUGIN_VERSION, PLUGIN_TAGS (the RP_* hooks). */
const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sine");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sine");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sine");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
