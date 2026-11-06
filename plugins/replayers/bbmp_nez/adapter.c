/* bbmp nez replayer (row I710w): a RENDER-kind adapter over libnezplug (third_party/libnezplug, Unlicense / MIT-0) for SGC (Sega Master
 * System / Game Gear / ColecoVision) and Game Boy GBR songs. Neither format carries a song length; the adapter plays a fixed length
 * (kLengthMs) with a short linear fade at the end. Subsongs are the file's song numbers (an SGC file holds a game's sound effects among
 * them). The ColecoVision BIOS is not available, so songs that call it are silent. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* libnezplug declares NEZNew() without a prototype; the vendored header stays untouched. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#include "nezplug/nezplug.h"
#pragma clang diagnostic pop

enum { kLengthMs = 180000, kFadeMs = 6000, kChunk = 1024 };

typedef struct bbnez_state {
    const sel_host* host;
    NEZ_PLAY* nez;
    uint32_t first, count;
    uint32_t rate;
    uint64_t pos, total;
    int is_sgc;
    char title[96], artist[96];
    int16_t buf[kChunk * 2];
    sel_tag meta[6];
} bbnez_state;

static int is_sgc(const uint8_t* d, size_t n) { return n >= 0x50u && memcmp(d, "SGC\x1a", 4) == 0; }
static int is_gbr(const uint8_t* d, size_t n) { return n >= 0x20u && memcmp(d, "GBRF", 4) == 0; }

/* Content probe: "SGC\x1a" or "GBRF" at the start (the extension alone is not enough). */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return is_sgc(data, size) || is_gbr(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void copy_text(char* dst, size_t cap, const char* src) {
    size_t n = 0;
    if (src != NULL) {
        while (n + 1u < cap && src[n] != '\0') {
            dst[n] = src[n];
            n++;
        }
    }
    dst[n] = '\0';
}

static void rp_close(void* state) {
    bbnez_state* s = (bbnez_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        if (s->nez != NULL) {
            NEZDelete(s->nez);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbnez_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < 0x20u || !rp_probe(src->data, src->size, src->path) ||
        src->size > 0x400000u) {
        return NULL;
    }
    s = (bbnez_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->is_sgc = is_sgc(src->data, src->size);
    s->nez = NEZNew();
    if (s->nez == NULL || NEZLoad(s->nez, src->data, (uint32_t)src->size) != 0) {
        rp_close(s);
        return NULL;
    }
    NEZSetChannel(s->nez, 2);
    NEZSetLength(s->nez, kLengthMs);
    NEZSetFade(s->nez, 0);
    s->first = NEZGetSongStart(s->nez);
    s->count = NEZGetSongMax(s->nez) >= s->first ? NEZGetSongMax(s->nez) - s->first + 1u : 1u;
    if (s->count > 255u) {
        s->count = 255u;
    }
    copy_text(s->title, sizeof s->title, NEZGetGameTitle(s->nez));
    copy_text(s->artist, sizeof s->artist, NEZGetGameArtist(s->nez));
    info->voice_count = 0;
    info->subsong_count = s->count;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbnez_state* s = (bbnez_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= s->count) {
        return 0;
    }
    s->rate = rate;
    s->total = (uint64_t)kLengthMs * rate / 1000u;
    s->pos = 0;
    NEZSetFrequency(s->nez, rate);
    NEZSetSongNo(s->nez, s->first + subsong);
    NEZReset(s->nez);
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbnez_state* s = (bbnez_state*)state;
    uint32_t done = 0;
    const uint64_t fade_frames = (uint64_t)kFadeMs * s->rate / 1000u;
    while (done < frames && s->pos < s->total) {
        uint32_t want = frames - done, i;
        if (want > (uint32_t)kChunk) {
            want = (uint32_t)kChunk;
        }
        if ((uint64_t)want > s->total - s->pos) {
            want = (uint32_t)(s->total - s->pos);
        }
        NEZRender(s->nez, s->buf, want);
        for (i = 0; i < want; i++) {
            uint64_t p = s->pos + i;
            int32_t l = s->buf[i * 2u], r = s->buf[i * 2u + 1u];
            if (p + fade_frames > s->total) {
                int64_t left = (int64_t)(s->total - p);
                l = (int32_t)((int64_t)l * left / (int64_t)fade_frames);
                r = (int32_t)((int64_t)r * left / (int64_t)fade_frames);
            }
            out[(size_t)(done + i) * 2u] = (int16_t)l;
            out[(size_t)(done + i) * 2u + 1u] = (int16_t)r;
        }
        s->pos += want;
        done += want;
    }
    return done;
}

static int rp_seek(void* state, uint32_t ms) {
    bbnez_state* s = (bbnez_state*)state;
    uint64_t target = (uint64_t)ms * s->rate / 1000u;
    if (s->rate == 0u || target > s->total) {
        return 0;
    }
    if (target < s->pos) { /* the emulation only runs forward: restart the song */
        NEZReset(s->nez);
        s->pos = 0;
    }
    while (s->pos < target) {
        uint32_t n = target - s->pos > (uint64_t)kChunk ? (uint32_t)kChunk : (uint32_t)(target - s->pos);
        NEZRender(s->nez, s->buf, n);
        s->pos += n;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    bbnez_state* s = (bbnez_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->artist[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->artist;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->is_sgc ? "SGC" : "Game Boy Sound System");
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)kLengthMs;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_nez");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sgc,gbr");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_nez");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
