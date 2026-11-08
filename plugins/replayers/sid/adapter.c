/* SID replayer (design doc "Format coverage rule", row I434): a RENDER-kind adapter over
 * libsidplayfp with the reSIDfp engine (third_party/libsidplayfp + third_party/libresidfp, GPL-2),
 * the same engine bbmp uses. PSID and RSID files, the HVSC format. The file has no length: the song
 * plays until the host stops it (the song length database, I605, supplies HVSC lengths). Seek
 * renders and discards, as SID state cannot be jumped. */
#include "openroms.h"
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

/* Polly Tracker (I710ar, Aleksi Eeben 2005): a .mod that is no Amiga module. The song has no header and no
 * magic: it is the tracker's own save, which its module-to-SID tool puts behind a 962-byte C64 player at $0801.
 * No spec exists; the shape is read off the five Modland songs (all start with the byte 0xAE, 14 KB to 46 KB).
 * Claimed only for the name ".mod", the first byte, a size that fits the C64's memory behind the player and no
 * Amiga module signature at 1080 (M.K., M!K!, FLT4/8, 4CHN/6CHN/8CHN, nnCH, CD81, OCTA, OKTA). */
#define POLLY_LOAD     0x0801u
#define POLLY_PLAYER   962u
#define POLLY_HEADER   0x7Cu /* the RSID v2 header in front of the player */
#define POLLY_END_HIGH 43u   /* in the player: the high byte of the module's last address */
#define POLLY_TEXTS    0x16u /* the header's name, author and released texts, 3 x 32 bytes */
#define POLLY_MIN_SIZE 512u
#define POLLY_MAX_SIZE (0x10000u - POLLY_LOAD - (POLLY_PLAYER - 2u))

static int polly_has_ext(const char* name) {
    size_t n = name != NULL ? strlen(name) : 0u;
    return n >= 4u && name[n - 4u] == '.' && (name[n - 3u] | 0x20) == 'm' && (name[n - 2u] | 0x20) == 'o' &&
           (name[n - 1u] | 0x20) == 'd';
}

static int polly_has_amiga_magic(const uint8_t* d, size_t size) {
    static const char* const magic[] = {"M.K.", "M!K!", "FLT4", "FLT8", "4CHN", "6CHN", "8CHN", "CD81", "OCTA", "OKTA"};
    size_t i;
    if (size < 1084u) {
        return 0;
    }
    for (i = 0; i < sizeof magic / sizeof magic[0]; i++) {
        if (memcmp(d + 1080u, magic[i], 4) == 0) {
            return 1;
        }
    }
    return (d[1082] == 'C' && d[1083] == 'H' && d[1080] >= '1' && d[1080] <= '3' && d[1081] >= '0' && d[1081] <= '9');
}

static int polly_shape(const uint8_t* d, size_t size) {
    return size >= POLLY_MIN_SIZE && size <= POLLY_MAX_SIZE && d[0] == 0xAEu && !polly_has_amiga_magic(d, size);
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (size >= 0x76u && (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0)) {
        return SEL_PROBE_YES;
    }
    if (size > 0u && polly_has_ext(name) && polly_shape(data, size)) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

/* The song as an RSID: header and player, the module behind them, byte 43 of the player set to the high byte of
 * the module's last address, the three header texts cleared and the title set to the file's stem. The caller
 * frees the result with host->mem_free. */
static uint8_t* polly_tune(const sel_host* host, const uint8_t* mod, size_t size, const char* path, size_t* out_size) {
    size_t total = SID_POLLY_PLAYER_SIZE + size, end = POLLY_LOAD + (POLLY_PLAYER - 2u) + size - 1u;
    uint8_t* t;
    const char* stem = path != NULL ? path : "";
    const char* p;
    size_t n, i;
    t = (uint8_t*)host->mem_alloc(host, total, SEL_MEM_FAST);
    if (t == NULL) {
        return NULL;
    }
    memcpy(t, sid_shim_polly_player(), SID_POLLY_PLAYER_SIZE);
    memcpy(t + SID_POLLY_PLAYER_SIZE, mod, size);
    t[POLLY_HEADER + POLLY_END_HIGH] = (uint8_t)(end >> 8);
    memset(t + POLLY_TEXTS, 0, 96u);
    for (p = stem; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            stem = p + 1;
        }
    }
    n = strlen(stem);
    if (polly_has_ext(stem)) {
        n -= 4u;
    }
    if (n == 0u) {
        stem = "Polly Tracker";
        n = strlen(stem);
    }
    for (i = 0; i < n && i < 31u; i++) {
        t[POLLY_TEXTS + i] = (uint8_t)stem[i];
    }
    *out_size = total;
    return t;
}

static void copy_text(char* dst, const char* src) {
    strncpy(dst, src != NULL ? src : "", SID_TEXT - 1u);
    dst[SID_TEXT - 1u] = '\0';
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    sid_state* s;
    int polly = 0;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (sid_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    if (src->size >= 4u && memcmp(src->data, "PSID", 4) != 0 && memcmp(src->data, "RSID", 4) != 0 &&
        polly_shape(src->data, src->size)) {
        size_t tune_size = 0;
        uint8_t* tune = polly_tune(host, src->data, src->size, src->path, &tune_size);
        if (tune != NULL) {
            s->shim = sid_shim_open(tune, tune_size);
            host->mem_free(host, tune);
        }
        polly = s->shim != NULL;
    } else {
        s->shim = sid_shim_open(src->data, src->size);
    }
    if (s->shim == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    s->subsongs = sid_shim_songs(s->shim);
    s->default_subsong = sid_shim_start_song(s->shim);
    copy_text(s->title, sid_shim_info(s->shim, 0));
    copy_text(s->author, sid_shim_info(s->shim, 1));
    copy_text(s->release, sid_shim_info(s->shim, 2));
    copy_text(s->format, polly ? "Polly Tracker" : sid_shim_format(s->shim));
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t)"sid");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t)"sid,psid,mod");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_CFG_APPLIED, SEL_TAG_FN(rp_cfg_applied));
    set(&g_hooks[12], SEL_RP_MUTE, SEL_TAG_FN(rp_mute));
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t)"sid");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
