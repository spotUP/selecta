/* AdPlug replayer (design doc "Format coverage rule", row I696): a RENDER-kind adapter over AdPlug (the engine bbmp
 * uses for its adplug backend) with the Nuked OPL3 emulator. AdLib/OPL2 songs of many trackers and drivers. AdPlug
 * has no content probe, so the plugin claims by extension and the open decides. */
#include "shim.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AP_TEXT 128u
#define AP_SKIP_FRAMES 4096u

typedef struct adplug_state {
    const sel_host* host;
    adplug_shim* shim;
    uint32_t rate, subsong;
    uint64_t played;
    char title[AP_TEXT], author[AP_TEXT], format[AP_TEXT];
    sel_tag meta[8];
} adplug_state;

#define AP_EXTENSIONS "hsc,sng,imf,wlf,adlib,a2m,a2t,amd,xms,bam,cmf,adl,d00,d01,dfm,hsp,ksm,mad,mus,mdy,ims,mdi,sci,laa,mkj,cff,dmo,dtm,mtk,mtr,rad,rac,raw,sat,sa2,xad,bmf,lds,plx,rol,xsm,pis,msc,rix,mkf,jbm,got,sop,hsq,sqx,sdb,agd,ha2"

/* AdPlug dispatches on the extension and each player validates its own header in open: claim UNSURE on a listed
 * extension only. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    const char* dot = name != NULL ? strrchr(name, '.') : NULL;
    char ext[8];
    size_t n, i;
    const char* p;
    (void)data;
    if (dot == NULL || size < 8u || strlen(dot + 1) >= sizeof ext || dot[1] == '\0') {
        return SEL_PROBE_NO;
    }
    for (n = 0; dot[1 + n] != '\0'; n++) {
        ext[n] = (char)((dot[1 + n] >= 'A' && dot[1 + n] <= 'Z') ? dot[1 + n] + 32 : dot[1 + n]);
    }
    ext[n] = '\0';
    for (p = AP_EXTENSIONS; *p != '\0';) {
        for (i = 0; p[i] != ',' && p[i] != '\0'; i++) {
        }
        if (i == n && memcmp(p, ext, n) == 0) {
            return SEL_PROBE_UNSURE;
        }
        p += i + (p[i] == ',' ? 1u : 0u);
    }
    return SEL_PROBE_NO;
}

static void copy_text(char* dst, const char* src) {
    strncpy(dst, src != NULL ? src : "", AP_TEXT - 1u);
    dst[AP_TEXT - 1u] = '\0';
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    adplug_state* s;
    if (src->stream != NULL || src->data == NULL || src->path == NULL) {
        return NULL;
    }
    s = (adplug_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->shim = adplug_shim_open(src->data, src->size, src->path);
    if (s->shim == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    copy_text(s->title, adplug_shim_info(s->shim, 0));
    copy_text(s->author, adplug_shim_info(s->shim, 1));
    copy_text(s->format, adplug_shim_info(s->shim, 3));
    info->voice_count = 0;
    info->subsong_count = adplug_shim_subsongs(s->shim);
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    adplug_state* s = (adplug_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        adplug_shim_close(s->shim);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    adplug_state* s = (adplug_state*)state;
    (void)voices;
    if (rate == 0u || !adplug_shim_start(s->shim, rate, subsong)) {
        return 0;
    }
    s->rate = rate;
    s->subsong = subsong;
    s->played = 0;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    adplug_state* s = (adplug_state*)state;
    uint32_t n = adplug_shim_render(s->shim, out, frames);
    s->played += n;
    return n;
}

/* A register log cannot be jumped, so a seek restarts when it goes back and renders away the gap. */
static int rp_seek(void* state, uint32_t ms) {
    adplug_state* s = (adplug_state*)state;
    int16_t scratch[2u * AP_SKIP_FRAMES];
    uint64_t target = (uint64_t)ms * s->rate / 1000u;
    if (target < s->played && !rp_start(s, s->subsong, s->rate, NULL)) {
        return 0;
    }
    while (s->played < target) {
        uint64_t left = target - s->played;
        uint32_t want = left < AP_SKIP_FRAMES ? (uint32_t)left : AP_SKIP_FRAMES;
        if (rp_render(s, scratch, want) == 0u) {
            break;
        }
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    adplug_state* s = (adplug_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->format[0] != '\0' ? s->format : "AdPlug");
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "adplug");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) AP_EXTENSIONS);
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    /* Below every native or content-probed player: AdPlug claims on extension alone. */
    set(&g_hooks[11], SEL_RP_PRIORITY, 0);
    set(&g_hooks[12], SEL_RP_FORMATS, (uintptr_t) AP_EXTENSIONS);
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "adplug");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
