/* CHIPNSFX replayer (row I432, design 2026-10-26-online-sources section 2a): a RENDER-kind adapter over CNGSoft's
 * CHIPNSFX tracker code (third_party/chipnsfx, GPL-3.0-or-later; this plugin is a GPL module, see LICENSE). A .chp song
 * is text: three AY channels for the Amstrad CPC, ZX Spectrum and MSX. The tracker's own loader and sound code make
 * 8-bit stereo (ABC) at 44100 Hz in frames of 1/100 s; the adapter resamples by straight lines to the host rate,
 * takes away the steady level the 8-bit sound has (0 is silence) and mixes a quarter of each side into the other, as
 * bbmp's csrc/chipnsfx/bbmp-chipnsfx.c does. The tracker keeps its state in globals: one song at a time (open of a
 * second song while one is open is refused). The song plays once; its length is the score walked through once. */
#include "engine.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

#define CHP_GAIN 120.0
#define CHP_DC_RATE 0.0005
#define CHP_CROSS 0.25

typedef struct chp_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    chp_info info;
    uint32_t rate;
    /* resampler */
    double step, pos, dc[2];
    float prev[2], cur[2];
    int have;
    int at;
    long done;
    unsigned char t[CHP_FRAME_SAMPLES * 2];
    sel_tag meta[8];
    char format[16];
} chp_state;

static int g_open; /* the tracker's globals hold one song */

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size >= 15u && memcmp(data, "CHIPNSFX format", 15) == 0) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    chp_state* s;
    const char* why = NULL;
    if (src->stream != NULL || src->data == NULL || g_open) {
        return NULL;
    }
    s = (chp_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    if (!chp_load(src->data, src->size, &s->info, &why)) {
        host->mem_free(host, s);
        return NULL;
    }
    g_open = 1;
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    chp_state* s = (chp_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        g_open = 0;
        host->mem_free(host, s);
    }
}

/* Rewinds the song and the resampler; skips `skip` engine frames of sound. */
static int rewind_to(chp_state* s, long skip) {
    unsigned char scratch[CHP_FRAME_SAMPLES * 2];
    long i;
    chp_restart();
    for (i = 0; i < skip && i < s->info.frames; i++) {
        if (!chp_frame(scratch)) {
            break;
        }
    }
    s->done = i;
    s->pos = 0;
    s->dc[0] = s->dc[1] = 0;
    s->prev[0] = s->prev[1] = s->cur[0] = s->cur[1] = 0;
    s->have = 0;
    s->at = 0;
    return 1;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    chp_state* s = (chp_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    s->step = (double)(CHP_ENGINE_RATE) / (double)rate;
    return rewind_to(s, 0);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    chp_state* s = (chp_state*)state;
    uint32_t n = 0;
    while (n < frames) {
        double side[2];
        int c;
        while (s->pos >= 1.0 || !s->have) {
            if (s->at >= CHP_FRAME_SAMPLES || !s->have) {
                if (s->done >= s->info.frames || !chp_frame(s->t)) {
                    return n;
                }
                s->done++;
                s->at = 0;
            }
            s->prev[0] = s->cur[0];
            s->prev[1] = s->cur[1];
            for (c = 0; c < 2; c++) {
                double v = s->t[s->at * 2 + c];
                s->dc[c] += (v - s->dc[c]) * CHP_DC_RATE;
                s->cur[c] = (float)(v - s->dc[c]);
            }
            s->at++;
            if (s->have) {
                s->pos -= 1.0;
            }
            s->have = 1;
        }
        for (c = 0; c < 2; c++) {
            side[c] = (s->prev[c] + (s->cur[c] - s->prev[c]) * s->pos) * CHP_GAIN;
        }
        for (c = 0; c < 2; c++) {
            /* ABC is hard (A left only, C right only): a quarter of each side goes into the other */
            double v = side[c] * (1.0 - CHP_CROSS) + side[1 - c] * CHP_CROSS;
            out[n * 2u + (uint32_t)c] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        s->pos += s->step;
        n++;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    chp_state* s = (chp_state*)state;
    return rewind_to(s, (long)(ms / 10u));
}

static const sel_tag* rp_metadata(void* state) {
    chp_state* s = (chp_state*)state;
    uint32_t n = 0;
    if (s->info.title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->info.title;
    }
    strcpy(s->format, "CHIPNSFX");
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)(s->info.frames * 10);
    if (s->info.comment[0] != '\0') {
        s->meta[n].tag = SEL_META_MESSAGE;
        s->meta[n++].value = (uintptr_t)s->info.comment;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "chipnsfx");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "chp");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "chipnsfx");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
