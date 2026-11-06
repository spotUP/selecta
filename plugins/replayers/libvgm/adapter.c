/* libvgm replayer (design doc "Format coverage rule", row I431): a RENDER-kind adapter over libvgm (ValleyBell), the
 * engine bbmp uses for its vgm backend. VGM and gzip-packed VGZ, S98, DRO and GYM logs of FM, PSG and PCM chips.
 * The file knows its length (one play plus one loop pass, no fade), so the render ends by itself. */
#include "shim.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VGM_TEXT 128u
#define VGM_SKIP_FRAMES 4096u

typedef struct vgm_state {
    const sel_host* host;
    vgm_shim* shim;
    uint32_t rate;
    uint64_t played;
    char title[VGM_TEXT], author[VGM_TEXT], release[VGM_TEXT], format[VGM_TEXT];
    sel_tag meta[8];
} vgm_state;

/* CYM (I710l): a log of the register writes a Capcom CPS-1 arcade board made to its YM2151 (MAME and Kawaks), filed by Modland
 * as "Megadrive CYM". Pairs of register and value; register 0 (the chip has none) is one 1/60 s frame's wait and has no value.
 * Turned into a VGM 1.51 file for one YM2151 (3579545 Hz), the same conversion as bbmp's backends/cym.py, which libvgm plays. */
#define CYM_CLOCK 3579545u
#define CYM_FRAME 735u            /* 44100 / 60 samples */
#define CYM_HEADER 0x100u
#define CYM_MAX_FRAMES 216000u    /* an hour; a damaged file cannot ask for more */
#define CYM_MAX_BYTES (16u << 20)

static int ext_cym(const char* name) {
    size_t n = name != NULL ? strlen(name) : 0u;
    return n >= 4u && name[n - 4u] == '.' && (name[n - 3u] | 0x20) == 'c' && (name[n - 2u] | 0x20) == 'y' &&
           (name[n - 1u] | 0x20) == 'm';
}

static void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* Returns a malloc'd VGM image (size in *out_size) or NULL when the log has no YM2151 write or no time. */
static uint8_t* cym_to_vgm(const uint8_t* d, size_t size, size_t* out_size) {
    size_t i = 0, n = CYM_HEADER;
    uint32_t frames = 0, writes = 0;
    uint8_t* v;
    if (size > CYM_MAX_BYTES) {
        size = CYM_MAX_BYTES;
    }
    v = (uint8_t*)malloc(CYM_HEADER + size * 3u / 2u + 8u);
    if (v == NULL) {
        return NULL;
    }
    memset(v, 0, CYM_HEADER);
    while (i < size && frames < CYM_MAX_FRAMES) {
        if (d[i] == 0u) {
            v[n++] = 0x62u;
            frames++;
            i++;
        } else if (i + 1u < size) {
            v[n++] = 0x54u;
            v[n++] = d[i];
            v[n++] = d[i + 1u];
            writes++;
            i += 2u;
        } else {
            break;
        }
    }
    if (writes == 0u || frames == 0u) {
        free(v);
        return NULL;
    }
    v[n++] = 0x66u;
    memcpy(v, "Vgm ", 4);
    put32(v + 0x04, (uint32_t)(n - 4u));
    put32(v + 0x08, 0x151u);
    put32(v + 0x18, frames * CYM_FRAME);
    put32(v + 0x24, 60u);
    put32(v + 0x30, CYM_CLOCK);
    put32(v + 0x34, CYM_HEADER - 0x34u);
    *out_size = n;
    return v;
}

/* Content probe: "Vgm " (VGM), 1F 8B (VGZ and other gzip; the open decides), "S98" (S98), "DRO" magic of
 * DOSBox raw OPL, "GYMX" (GYM with header). A gzip that holds no VGM fails in open, never claims on extension. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (ext_cym(name)) {
        return size >= 2u ? SEL_PROBE_YES : SEL_PROBE_NO; /* no magic; open refuses a log without writes or time */
    }
    if (size >= 0x40u && memcmp(data, "Vgm ", 4) == 0) {
        return SEL_PROBE_YES;
    }
    if (size >= 18u && data[0] == 0x1fu && data[1] == 0x8bu) {
        return SEL_PROBE_UNSURE;
    }
    if (size >= 0x20u && memcmp(data, "S98", 3) == 0) {
        return SEL_PROBE_YES;
    }
    if (size >= 0x18u && memcmp(data, "DBRAWOPL", 8) == 0) {
        return SEL_PROBE_YES;
    }
    if (size >= 0x1a0u && memcmp(data, "GYMX", 4) == 0) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

static void copy_text(char* dst, const char* src) {
    strncpy(dst, src != NULL ? src : "", VGM_TEXT - 1u);
    dst[VGM_TEXT - 1u] = '\0';
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    vgm_state* s;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (vgm_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    if (ext_cym(src->path)) {
        size_t vsize = 0;
        uint8_t* vgm = cym_to_vgm(src->data, src->size, &vsize);
        if (vgm != NULL) {
            s->shim = vgm_shim_open(vgm, vsize);
            free(vgm);
        }
        copy_text(s->format, "CYM (Capcom YM2151 log)");
        if (s->shim == NULL) {
            host->mem_free(host, s);
            return NULL;
        }
    } else {
        s->shim = vgm_shim_open(src->data, src->size);
    }
    if (s->shim == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    copy_text(s->title, vgm_shim_info(s->shim, 0));
    copy_text(s->author, vgm_shim_info(s->shim, 1));
    copy_text(s->release, vgm_shim_info(s->shim, 2));
    if (s->format[0] == '\0') {
        copy_text(s->format, vgm_shim_format(s->shim));
    }
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    vgm_state* s = (vgm_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        vgm_shim_close(s->shim);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    vgm_state* s = (vgm_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u || !vgm_shim_start(s->shim, rate)) {
        return 0;
    }
    s->rate = rate;
    s->played = 0;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    vgm_state* s = (vgm_state*)state;
    uint32_t n = vgm_shim_render(s->shim, out, frames);
    s->played += n;
    return n;
}

/* A log cannot be jumped, so a seek restarts when it goes back and renders away the gap. */
static int rp_seek(void* state, uint32_t ms) {
    vgm_state* s = (vgm_state*)state;
    int16_t scratch[2u * VGM_SKIP_FRAMES];
    uint64_t target = (uint64_t)ms * s->rate / 1000u;
    if (target < s->played && !rp_start(s, 0, s->rate, NULL)) {
        return 0;
    }
    while (s->played < target) {
        uint64_t left = target - s->played;
        uint32_t want = left < VGM_SKIP_FRAMES ? (uint32_t)left : VGM_SKIP_FRAMES;
        if (rp_render(s, scratch, want) == 0u) {
            break;
        }
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    vgm_state* s = (vgm_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    if (s->release[0] != '\0') {
        s->meta[n].tag = SEL_META_RELEASE;
        s->meta[n++].value = (uintptr_t)s->release;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->format[0] != '\0' ? s->format : "VGM");
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "libvgm");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "vgm,vgz,s98,dro,gym,cym");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    /* Outranks gme for GYM; gme has no VGM core in Selecta (I862). */
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_RP_FORMATS, (uintptr_t) "vgm,vgz,s98,dro,gym,cym");
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "libvgm");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
