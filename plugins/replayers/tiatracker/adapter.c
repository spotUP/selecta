/* TIATracker replayer (row I429, design 2026-10-26-online-sources section 2a): a RENDER-kind adapter over the in-process
 * TIATracker engine (engine.cpp over third_party/tiatracker, GPL-2.0-only; this plugin is a GPL module, see LICENSE).
 * A .ttt song is JSON for the Atari 2600's TIA sound chip, two channels played at the console's frame rate (50 or 60 Hz).
 * The engine validates the song and measures its length at open; the song plays once until it ends or comes back to a
 * row it has played. Output is 16-bit interleaved stereo at the host rate. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "engine.h"

#include <stdlib.h>
#include <string.h>

#define TIA_SKIP_CHUNK 1024

typedef struct tia_state {
    const sel_host* host;
    tia_engine* engine;
    uint32_t rate;
    char title[128];
    char author[128];
    sel_tag meta[8];
    char format[16];
} tia_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    static const char key[] = "\"channels\"";
    size_t i, lim = size < 4096u ? size : 4096u;
    (void)name;
    if (lim == 0u) {
        return SEL_PROBE_NO;
    }
    for (i = 0; i < lim && (data[i] == ' ' || data[i] == '\t' || data[i] == '\r' || data[i] == '\n'); i++) {
    }
    if (i >= lim || data[i] != '{') {
        return SEL_PROBE_NO;
    }
    for (; i + sizeof key - 1u <= size && i < lim; i++) {
        if (memcmp(data + i, key, sizeof key - 1u) == 0) {
            return SEL_PROBE_UNSURE;
        }
    }
    return SEL_PROBE_NO;
}

static void copy_ascii(char* dst, size_t cap, const char* src) {
    size_t i, n = 0;
    for (i = 0; src[i] != '\0' && n + 1u < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 0x20u && c != 0x7Fu) {
            dst[n++] = (char)c; /* UTF-8 passes through */
        }
    }
    dst[n] = '\0';
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    tia_state* s;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (tia_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->engine = tia_open(src->data, src->size);
    if (s->engine == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    copy_ascii(s->title, sizeof s->title, tia_title(s->engine));
    copy_ascii(s->author, sizeof s->author, tia_author(s->engine));
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    tia_state* s = (tia_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        tia_close(s->engine);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    tia_state* s = (tia_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    return tia_start(s->engine, rate);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    tia_state* s = (tia_state*)state;
    return tia_render(s->engine, out, frames);
}

static int rp_seek(void* state, uint32_t ms) {
    tia_state* s = (tia_state*)state;
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    int16_t skip[TIA_SKIP_CHUNK * 2];
    if (!tia_start(s->engine, s->rate)) {
        return 0;
    }
    while (left > 0u) {
        uint32_t k = left < TIA_SKIP_CHUNK ? (uint32_t)left : TIA_SKIP_CHUNK;
        uint32_t got = tia_render(s->engine, skip, k);
        if (got == 0u) {
            break;
        }
        left -= got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    tia_state* s = (tia_state*)state;
    uint32_t n = 0;
    uint32_t ms = tia_duration_ms(s->engine);
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    strcpy(s->format, "TIATracker");
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
    if (ms > 0u) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)ms;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "tiatracker");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "ttt");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "tiatracker");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
