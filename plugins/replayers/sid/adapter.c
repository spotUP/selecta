/* SID replayer (design doc "Format coverage rule", row I434): a RENDER-kind adapter over libsidplayfp with the
 * reSIDfp engine (third_party/libsidplayfp + third_party/libresidfp, GPL-2), the same engine bbmp uses. PSID and RSID
 * files, the HVSC format. The file has no length: the song plays until the host stops it (the song length database,
 * I605, supplies HVSC lengths). Seek renders and discards, as SID state cannot be jumped. */
#include "shim.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

#define SID_TEXT 128u
#define SID_SKIP_FRAMES 4096u

typedef struct sid_state {
    const sel_host* host;
    sid_shim* shim;
    uint32_t subsongs, default_subsong, rate, song;
    uint64_t played;
    char title[SID_TEXT], author[SID_TEXT], release[SID_TEXT], format[SID_TEXT];
    sel_tag meta[8];
} sid_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size >= 0x76u && (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0)) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

static void copy_text(char* dst, const char* src) {
    strncpy(dst, src != NULL ? src : "", SID_TEXT - 1u);
    dst[SID_TEXT - 1u] = '\0';
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    sid_state* s;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (sid_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->shim = sid_shim_open(src->data, src->size);
    if (s->shim == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    s->subsongs = sid_shim_songs(s->shim);
    s->default_subsong = sid_shim_start_song(s->shim);
    copy_text(s->title, sid_shim_info(s->shim, 0));
    copy_text(s->author, sid_shim_info(s->shim, 1));
    copy_text(s->release, sid_shim_info(s->shim, 2));
    copy_text(s->format, sid_shim_format(s->shim));
    info->voice_count = 0;
    info->subsong_count = s->subsongs > 0u ? s->subsongs : 1u;
    info->default_subsong = s->default_subsong > 0u ? s->default_subsong - 1u : 0u;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    sid_state* s = (sid_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        sid_shim_close(s->shim);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    sid_state* s = (sid_state*)state;
    (void)voices;
    if (rate == 0u || subsong >= (s->subsongs > 0u ? s->subsongs : 1u)) {
        return 0;
    }
    if (!sid_shim_start(s->shim, subsong + 1u, rate)) {
        return 0;
    }
    s->rate = rate;
    s->song = subsong;
    s->played = 0;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    sid_state* s = (sid_state*)state;
    uint32_t n = sid_shim_render(s->shim, out, frames);
    s->played += n;
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    sid_state* s = (sid_state*)state;
    int16_t scratch[2u * SID_SKIP_FRAMES];
    uint64_t target = (uint64_t)ms * s->rate / 1000u;
    if (target < s->played && !rp_start(s, s->song, s->rate, NULL)) {
        return 0;
    }
    while (s->played < target) {
        uint64_t left = target - s->played;
        uint32_t want = left < SID_SKIP_FRAMES ? (uint32_t)left : SID_SKIP_FRAMES;
        if (rp_render(s, scratch, want) == 0u) {
            break;
        }
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    sid_state* s = (sid_state*)state;
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
    s->meta[n++].value = (uintptr_t)(s->format[0] != '\0' ? s->format : "SID");
    s->meta[n].tag = SEL_META_SUBSONGS;
    s->meta[n++].value = (uintptr_t)s->subsongs;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sid");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sid,psid");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sid");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
