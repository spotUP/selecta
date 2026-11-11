/* pxtone replayer (row I702): a RENDER-kind adapter over pxtone (third_party/pxtone, Studio Pixel's pxtone play sample
 * sources, free to use and modify with no attribution duty) for Pixel's PiSTOL Collage songs (.ptcop projects and
 * .pttune exports). The service mixes at the host rate; the song plays once to its end (loop flag off), the safety stop
 * is 20 minutes. Woices that embed Ogg Vorbis are not decoded (pxINCLUDE_OGGVORBIS off): those units stay silent.
 * The wave and noise woices a song carries are inside the file, so there are no companion files. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "probe.h"

#include <stdlib.h>
#include <string.h>

struct pxshim;
struct pxshim* pxshim_open(const uint8_t* data, uint32_t size, uint32_t rate);
uint32_t pxshim_render(struct pxshim* m, int16_t* out, uint32_t frames);
const char* pxshim_title(const struct pxshim* m);
void pxshim_close(struct pxshim* m);

#define PX_MAX_SECONDS 1200u

typedef struct px_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    struct pxshim* shim;
    uint32_t rate;
    uint64_t frames;
    int ended;
    char title[64];
    sel_tag meta[3];
} px_state;

static void rp_close(void* state) {
    px_state* s = (px_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        pxshim_close(s->shim);
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    px_state* s;
    if (src->stream != NULL || src->data == NULL || px_probe(src->data, src->size, NULL) != SEL_PROBE_YES || src->size > 0x7FFFFFFFu) {
        return NULL;
    }
    s = (px_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)host->mem_alloc(host, src->size, SEL_MEM_FAST);
    if (s->data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int restart(px_state* s) {
    const char* t;
    size_t i, n = 0;
    pxshim_close(s->shim);
    s->shim = pxshim_open(s->data, (uint32_t)s->size, s->rate);
    s->frames = 0;
    s->ended = 0;
    if (s->shim == NULL) {
        return 0;
    }
    /* Titles are Shift-JIS: keep the ASCII ones, drop the rest rather than show mojibake. */
    t = pxshim_title(s->shim);
    for (i = 0; t[i] != '\0' && n < sizeof s->title - 1u; i++) {
        if ((unsigned char)t[i] < 0x20u || (unsigned char)t[i] > 0x7Eu) {
            n = 0;
            break;
        }
        s->title[n++] = t[i];
    }
    s->title[n] = '\0';
    return 1;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    px_state* s = (px_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    return restart(s);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    px_state* s = (px_state*)state;
    uint32_t n;
    if (s->shim == NULL || s->ended) {
        return 0;
    }
    n = pxshim_render(s->shim, out, frames);
    s->frames += n;
    if (n < frames || s->frames >= (uint64_t)s->rate * PX_MAX_SECONDS) {
        s->ended = 1;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    px_state* s = (px_state*)state;
    int16_t scratch[2048 * 2];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    if (s->shim == NULL || !restart(s)) {
        return 0;
    }
    /* The service can start at a position only through its prepare call; restart and mix forward, discarding the output. */
    while (left > 0u && !s->ended) {
        uint32_t want = left > 2048u ? 2048u : (uint32_t)left;
        uint32_t got = rp_render(s, scratch, want);
        if (got == 0u) {
            break;
        }
        left -= got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    px_state* s = (px_state*)state;
    size_t k = 0;
    s->meta[k].tag = SEL_META_FORMAT;
    s->meta[k++].value = (uintptr_t) "pxtone";
    if (s->title[0] != '\0') {
        s->meta[k].tag = SEL_META_TITLE;
        s->meta[k++].value = (uintptr_t)s->title;
    }
    s->meta[k].tag = SEL_TAG_END;
    s->meta[k].value = 0;
    return s->meta;
}

static sel_tag g_hooks[14];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    {
        size_t n = px_static_hooks(g_hooks); /* probe.c: shared with the Amiga manifest (I427e4) */
        set(&g_hooks[n++], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
        set(&g_hooks[n++], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
        set(&g_hooks[n++], SEL_RP_START, SEL_TAG_FN(rp_start));
        set(&g_hooks[n++], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
        set(&g_hooks[n++], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
        set(&g_hooks[n++], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
        set(&g_hooks[n], SEL_TAG_END, 0);
    }

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "pxtone");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
