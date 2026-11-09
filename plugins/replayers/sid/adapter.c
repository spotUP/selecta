/* SID replayer (design doc "Format coverage rule", row I434): a RENDER-kind adapter over
 * libsidplayfp with the reSIDfp engine (third_party/libsidplayfp + third_party/libresidfp, GPL-2),
 * the same engine bbmp uses. PSID and RSID files, the HVSC format. The file has no length: the song
 * plays until the host stops it (the song length database, I605, supplies HVSC lengths). Seek
 * renders and discards, as SID state cannot be jumped. */
#include "shim.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SID_TEXT        128u
#define SID_SKIP_FRAMES 4096u

typedef struct sid_state {
    const sel_host* host;
    sid_shim* shim;
    uint32_t subsongs, default_subsong, rate, song;
    uint64_t played;
    char title[SID_TEXT], author[SID_TEXT], release[SID_TEXT], format[SID_TEXT];
    sel_tag meta[8];
    uint8_t kernal[8192], basic[8192],
        chargen[4096]; /* config-dir overrides, valid when the matching have_* is 1 */
    int have_kernal, have_basic, have_chargen;
} sid_state;

static const char* cfg(const sel_host* host, const char* key, const char* def) {
    const char* v = host->config_get != NULL ? host->config_get(host, key, def) : def;
    return v != NULL ? v : def;
}

/* Whole file of exactly `size` bytes from <dir>/<name>; 0 when absent or another size. */
static int load_rom(const char* dir, const char* name, uint8_t* out, size_t size) {
    char path[512];
    FILE* f;
    size_t n;
    int ok;
    if (dir == NULL || dir[0] == '\0' ||
        snprintf(path, sizeof path, "%s/%s", dir, name) >= (int)sizeof path) {
        return 0;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    n = fread(out, 1, size, f);
    ok = n == size && fgetc(f) == EOF;
    fclose(f);
    return ok;
}

/* The chip, clock, filter and ROM settings of this start (MPS-1). A ROM file in the config
 * directory beats the embedded OpenROMs image. */
static void make_cfg(sid_state* s, sid_shim_cfg* c) {
    const sel_host* h = s->host;
    const char* v;
    memset(c, 0, sizeof *c);
    v = cfg(h, "sid.model", "auto");
    c->model = strcmp(v, "6581") == 0 ? 1 : strcmp(v, "8580") == 0 ? 2 : 0;
    v = cfg(h, "sid.clock", "auto");
    c->clock = strcmp(v, "pal") == 0 ? 1 : strcmp(v, "ntsc") == 0 ? 2 : 0;
    c->filter = strcmp(cfg(h, "sid.filter", "on"), "off") != 0;
    v = cfg(h, "sid.digiboost", "auto");
    c->digiboost = strcmp(v, "on") == 0 ? 1 : strcmp(v, "off") == 0 ? 2 : 0;
    {
        const char* dir = cfg(h, "config.dir", "");
        s->have_kernal = load_rom(dir, "kernal", s->kernal, sizeof s->kernal);
        s->have_basic = load_rom(dir, "basic", s->basic, sizeof s->basic);
        s->have_chargen = load_rom(dir, "chargen", s->chargen, sizeof s->chargen);
    }
    c->kernal = s->have_kernal ? s->kernal : sid_shim_builtin_kernal();
    c->basic = s->have_basic ? s->basic : sid_shim_builtin_basic();
    c->chargen = s->have_chargen ? s->chargen : sid_shim_builtin_chargen();
}

static uint32_t rp_cfg_applied(void* state) {
    return sid_shim_cfg_applied(((sid_state*)state)->shim);
}

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
    /* Four mute channels per chip: voices 0-2, then the chip's digi samples. RENDER kind, so no
     * engine voices: the count sizes SOLO and the mute mask only. */
    info->voice_count = 4u * sid_shim_chips(s->shim);
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
    sid_shim_cfg c;
    (void)voices;
    if (rate == 0u || subsong >= (s->subsongs > 0u ? s->subsongs : 1u)) {
        return 0;
    }
    make_cfg(s, &c);
    if (!sid_shim_start(s->shim, subsong + 1u, rate, &c)) {
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

/* RP_MUTE: channel = chip * 4 + voice (voice 3 is the chip's digi samples). */
static void rp_mute(void* state, uint32_t channel, uint32_t muted) {
    sid_shim_mute(((sid_state*)state)->shim, channel / 4u, channel % 4u, muted);
}

/* Register feed (I1226): libsidplayfp's last-written register mirror of each chip (1 to 3, the tune's SID count), the
 * same contract as the playsid adapter. The envelope levels stay 0: reSIDfp keeps its envelope counters private. */
static uint32_t rp_viz_sid(void* state, uint8_t* regs, uint8_t* env, uint32_t max_chips) {
    sid_state* s = (sid_state*)state;
    uint32_t chips = sid_shim_chips(s->shim);
    (void)env;
    if (chips > max_chips) chips = max_chips;
    return sid_shim_regs(s->shim, regs, chips);
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

static sel_tag g_hooks[15];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t)"sid");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t)"sid,psid");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_CFG_APPLIED, SEL_TAG_FN(rp_cfg_applied));
    set(&g_hooks[12], SEL_RP_MUTE, SEL_TAG_FN(rp_mute));
    set(&g_hooks[13], SEL_RP_VIZ_SID, SEL_TAG_FN(rp_viz_sid));
    set(&g_hooks[14], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t)"sid");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
