/* bbmp darkwave replayer (row I710h): a RENDER-kind adapter over bbmp's csrc/darkwave (third_party/bbmp_darkwave, DarkWave Studio
 * 4.1.6 engine, GPL-3.0-or-later). The studio is built per sample rate and keeps its state in one global, so open() validates and
 * measures at 44100 Hz, and RP_START rebuilds it at the host rate and renders the whole song into memory. The studio's loader
 * validates the file, so a file that is not a DarkWave Studio project fails there. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dw_engine.h"

enum { kChunk = 4096, kMaxFrames = 3600 * 192000 };

typedef struct bbdw_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    long duration_ms;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} bbdw_state;

static int ext_is_dwp(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 5u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 'd' && (name[nl - 2u] | 0x20) == 'w' &&
           (name[nl - 1u] | 0x20) == 'p';
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (size < 16u || memcmp(data, " PWD", 4) != 0) {
        return SEL_PROBE_NO;
    }
    /* The signature is enough; the extension only raises confidence. The engine's loader validates the rest. */
    return ext_is_dwp(name) ? SEL_PROBE_YES : SEL_PROBE_UNSURE;
}

static void rp_close(void* state) {
    bbdw_state* s = (bbdw_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        dw_close();
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbdw_state* s;
    dw_info* bi;
    int ok;
    if (src->stream != NULL || src->data == NULL || src->size < 16u) {
        return NULL;
    }
    s = (bbdw_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    bi = (dw_info*)calloc(1, sizeof *bi);
    if (s == NULL || bi == NULL) {
        free(bi);
        if (s != NULL) {
            host->mem_free(host, s);
        }
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)malloc(src->size);
    if (s->data == NULL) {
        free(bi);
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    ok = dw_open(s->data, s->size, 44100u, bi);
    if (!ok) {
        free(bi);
        free(s->data);
        host->mem_free(host, s);
        return NULL;
    }
    dw_close();
    s->duration_ms = bi->duration_ms;
    free(bi);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbdw_state* s = (bbdw_state*)state;
    dw_info* bi;
    int16_t* native = NULL;
    uint64_t n = 0, cap = 0;
    int ok;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->frames = s->pos = 0;
    s->rate = rate;
    bi = (dw_info*)calloc(1, sizeof *bi);
    if (bi == NULL) {
        return 0;
    }
    ok = dw_open(s->data, s->size, rate, bi);
    free(bi);
    if (!ok || !dw_start()) {
        dw_close();
        return 0;
    }
    for (;;) {
        int16_t buf[kChunk * 2];
        uint32_t got = dw_render(buf, kChunk);
        if (got > 0u) {
            if (n + got > cap) {
                int16_t* grown;
                cap = cap == 0u ? 1u << 20 : cap * 2u;
                grown = (int16_t*)realloc(native, (size_t)cap * 4u);
                if (grown == NULL) {
                    ok = 0;
                    break;
                }
                native = grown;
            }
            memcpy(native + n * 2u, buf, (size_t)got * 4u);
            n += got;
        }
        if (got < kChunk || n >= (uint64_t)kMaxFrames) {
            break;
        }
    }
    dw_close();
    if (!ok || n == 0u) {
        free(native);
        return 0;
    }
    s->pcm = native;
    s->frames = n;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbdw_state* s = (bbdw_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbdw_state* s = (bbdw_state*)state;
    if (s->pcm == NULL) {
        return 0;
    }
    s->pos = (uint64_t)ms * s->rate / 1000u;
    if (s->pos > s->frames) {
        s->pos = s->frames;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    bbdw_state* s = (bbdw_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "DarkWave Studio";
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->duration_ms;
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[13];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_darkwave");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "dwp");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_darkwave");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
