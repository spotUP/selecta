/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ZXTune replayer (design doc "Format coverage rule", row I421): a RENDER-kind adapter over ZXTune
 * (LGPL-3.0, built from the upstream revision bbmp uses, with bbmp's patches). Spectrum and related trackers:
 * Pro Tracker 1/2/3, Sound Tracker (STC, ST1, ST3), Sound Tracker Pro, ASC Sound Master, SQ Tracker, Pro Sound
 * Creator, Pro Sound Maker, Vortex (VTX), PSG dumps, Sam Coupe COP, Digital Studio (DST, CHI, PDT, ...), TurboFM
 * and Multi-Track Container files. ZXTune identifies the format from the content; the extension list only decides
 * which files are offered to the probe. A song with no stored length plays the ZXTune duration, then ends. */
#include "shim.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

#define ZX_OPEN_RATE 44100u

typedef struct zx_state {
    const sel_host* host;
    zx_shim* shim;
    uint32_t tracks, rate;
    uint64_t played, limit;
    sel_tag meta[8];
} zx_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size < 16u) {
        return SEL_PROBE_NO;
    }
    return zx_shim_probe(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    zx_state* s;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (zx_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->shim = zx_shim_open(src->data, src->size);
    if (s->shim == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    s->tracks = zx_shim_tracks(s->shim);
    info->voice_count = 0;
    info->subsong_count = s->tracks != 0u ? s->tracks : 1u;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    zx_state* s = (zx_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        zx_shim_close(s->shim);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    zx_state* s = (zx_state*)state;
    (void)voices;
    if (rate == 0u || subsong >= s->tracks || !zx_shim_start(s->shim, subsong, rate)) {
        return 0;
    }
    s->rate = rate;
    s->played = 0;
    s->limit = (uint64_t)zx_shim_duration_ms(s->shim) * rate / 1000u;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    zx_state* s = (zx_state*)state;
    uint32_t got;
    if (s->limit != 0u) {
        uint64_t left = s->limit > s->played ? s->limit - s->played : 0u;
        if (left == 0u) {
            return 0;
        }
        if (frames > left) {
            frames = (uint32_t)left;
        }
    }
    got = zx_shim_render(s->shim, out, frames);
    s->played += got;
    return got;
}

static int rp_seek(void* state, uint32_t ms) {
    zx_state* s = (zx_state*)state;
    if (!zx_shim_seek(s->shim, ms)) {
        return 0;
    }
    s->played = (uint64_t)ms * s->rate / 1000u;
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    zx_state* s = (zx_state*)state;
    const char* title = zx_shim_info(s->shim, 0);
    const char* author = zx_shim_info(s->shim, 1);
    const char* type = zx_shim_info(s->shim, 3);
    const char* comment = zx_shim_info(s->shim, 4);
    uint32_t n = 0;
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(type[0] != '\0' ? type : "ZXTune");
    s->meta[n].tag = SEL_META_SUBSONGS;
    s->meta[n++].value = (uintptr_t)(s->tracks != 0u ? s->tracks : 1u);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)zx_shim_duration_ms(s->shim);
    if (comment[0] != '\0') {
        s->meta[n].tag = SEL_META_MESSAGE;
        s->meta[n++].value = (uintptr_t)comment;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "zxtune");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS,
        (uintptr_t) "pt1,pt2,pt3,stc,st1,st3,stp,asc,as0,sqt,psc,psm,pse,ts,ftc,gtr,fxm,psg,vtx,pdt,chi,str,dst,"
                    "dmm,ptd,sqd,cop,tfc,tfd,tfe,tf0,mtc,hob");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "zxtune");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
