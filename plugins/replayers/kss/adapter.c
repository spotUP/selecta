/* libkss replayer (row I698, design 2026-10-26-online-sources section 2a): a RENDER-kind adapter over libkss
 * (third_party/libkss, permissive licence, plain C) for MSX music: KSS rips (KSCC/KSSX) and the MSX sound drivers' own
 * formats, which libkss wraps in a KSS image with the driver binary: MGSDRV .mgs, MoonBlaster .mbm, MuSICA .bgm,
 * OPLLDRV .opx, MPK .mpk. The song is handed over as bytes (KSS_bin2kss); MoonBlaster songs look for their .MBK sample
 * kit next to the file, so a path helps there. Priority 10: above the gme plugin, which also claims .kss. A song plays
 * its loop once, or until libkss reports the end (stop flag, or silence for 12 s). */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "kss/kss.h"
#include "kssplay.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define KSS_SILENT_LIMIT_MS 12000u
#define KSS_SKIP_CHUNK 4096u

typedef struct kss_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    KSS* kss;
    KSSPLAY* play;
    uint32_t song_first;
    uint32_t subsongs;
    uint32_t rate;
    uint32_t length_ms;
    int ended;
    char title[256];
    sel_tag meta[8];
    char format[8];
} kss_state;

static int ext_is(const char* name, const char* ext) {
    const char* dot;
    size_t i;
    if (name == NULL || (dot = strrchr(name, '.')) == NULL) {
        return 0;
    }
    for (i = 0; ext[i] != '\0'; i++) {
        char c = dot[1 + i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + ('a' - 'A'));
        }
        if (c != ext[i]) {
            return 0;
        }
    }
    return dot[1 + i] == '\0';
}

static int has_driver_ext(const char* name) {
    return ext_is(name, "mgs") || ext_is(name, "mbm") || ext_is(name, "bgm") || ext_is(name, "opx") ||
           ext_is(name, "mpk");
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    int type;
    if (size < 4u) {
        return SEL_PROBE_NO;
    }
    /* KSS magic is certain. The driver formats are recognised by heuristics (and .mbm by name only), so those also
     * need their extension: a stray file must not become a "song". */
    if (memcmp(data, "KSCC", 4) == 0 || memcmp(data, "KSSX", 4) == 0) {
        return SEL_PROBE_YES;
    }
    if (!has_driver_ext(name)) {
        return SEL_PROBE_NO;
    }
    type = KSS_check_type((uint8_t*)data, (uint32_t)size, name);
    return (type == MGSDATA || type == MBMDATA || type == MPK106DATA || type == MPK103DATA || type == BGMDATA ||
            type == OPXDATA)
               ? SEL_PROBE_YES
               : SEL_PROBE_NO;
}

static void release(kss_state* s) {
    if (s->play != NULL) {
        KSSPLAY_delete(s->play);
        s->play = NULL;
    }
}

static void rp_close(void* state) {
    kss_state* s = (kss_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        release(s);
        if (s->kss != NULL) {
            KSS_delete(s->kss);
        }
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    kss_state* s;
    size_t i, n = 0;
    const uint8_t* t;
    if (src->stream != NULL || src->data == NULL || src->size < 4u || src->size > 0x1000000u) {
        return NULL;
    }
    s = (kss_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)host->mem_alloc(host, src->size, SEL_MEM_FAST);
    if (s->data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    if (src->path != NULL && ext_is(src->path, "mbm")) {
        (void)KSS_autoload_mbk(src->path, "", "");
    }
    s->kss = KSS_bin2kss(s->data, (uint32_t)s->size, src->path);
    if (s->kss == NULL) {
        rp_close(s);
        return NULL;
    }
    if (s->kss->info != NULL && s->kss->info_num > 0u) {
        s->subsongs = s->kss->info_num;
        s->song_first = (uint32_t)s->kss->info[0].song;
        s->length_ms = s->kss->info[0].time_in_ms > 0 ? (uint32_t)s->kss->info[0].time_in_ms : 0u;
    } else if (s->kss->trk_max > s->kss->trk_min) {
        s->subsongs = (uint32_t)s->kss->trk_max - s->kss->trk_min + 1u;
        s->song_first = s->kss->trk_min;
    } else {
        s->subsongs = 1u;
        s->song_first = s->kss->trk_min;
    }
    /* Titles are Shift-JIS; keep the ASCII part, which a UTF-8 UI shows as is. */
    for (t = s->kss->title, i = 0; t[i] != '\0' && i < KSS_TITLE_MAX && n + 1u < sizeof s->title; i++) {
        if (t[i] >= 0x20u && t[i] < 0x7Fu) {
            s->title[n++] = (char)t[i];
        }
    }
    while (n > 0u && s->title[n - 1u] == ' ') {
        n--;
    }
    s->title[n] = '\0';
    info->voice_count = 0;
    info->subsong_count = s->subsongs;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int start_song(kss_state* s, uint32_t subsong) {
    uint32_t song = s->song_first + subsong;
    if (s->kss->info != NULL && subsong < s->kss->info_num) {
        song = (uint32_t)s->kss->info[subsong].song;
    }
    release(s);
    s->play = KSSPLAY_new(s->rate, 2, 16);
    if (s->play == NULL) {
        return 0;
    }
    KSSPLAY_set_data(s->play, s->kss);
    KSSPLAY_reset(s->play, song, 0);
    KSSPLAY_set_silent_limit(s->play, KSS_SILENT_LIMIT_MS);
    s->ended = 0;
    return 1;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    kss_state* s = (kss_state*)state;
    (void)voices;
    if (rate == 0u || subsong >= s->subsongs) {
        return 0;
    }
    s->rate = rate;
    return start_song(s, subsong);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    kss_state* s = (kss_state*)state;
    if (s->play == NULL || s->ended) {
        return 0;
    }
    KSSPLAY_calc(s->play, out, frames);
    if (KSSPLAY_get_stop_flag(s->play) || KSSPLAY_get_fade_flag(s->play) == KSSPLAY_FADE_END ||
        (s->kss->loop_detectable && KSSPLAY_get_loop_count(s->play) >= 1)) {
        s->ended = 1;
    }
    return frames;
}

static int rp_seek(void* state, uint32_t ms) {
    kss_state* s = (kss_state*)state;
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    if (s->play == NULL || s->rate == 0u) {
        return 0;
    }
    /* The player has no random access: restart the song and run the emulation forward without mixing. */
    if (!start_song(s, 0)) {
        return 0;
    }
    while (left > 0u) {
        uint32_t k = left < KSS_SKIP_CHUNK ? (uint32_t)left : KSS_SKIP_CHUNK;
        KSSPLAY_calc_silent(s->play, k);
        left -= k;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    kss_state* s = (kss_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    strcpy(s->format, "KSS");
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
    if (s->length_ms > 0u) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)s->length_ms;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

/* RP_CONTENT_COMPANIONS (I1127b): a MoonBlaster .mbm needs its sample kit (.MBK). libkss searches `<stem>.MBK`, then the
 * 8-character name at MBM header offset 0x140 (stop at space or NUL) + ".MBK" (mbm2kss.c:155). Parsed here, never through
 * KSS_autoload_mbk (static path buffer: not reentrant). Local open keeps KSS_autoload_mbk. */
static unsigned rp_content_companions(const char* name, const uint8_t* data, size_t size, char out[][SEL_RP_COMPANION_NAME_MAX],
                                      unsigned max, const sel_host* host) {
    size_t nl = strlen(name), stem, i, n = 0;
    unsigned k = 0;
    (void)host;
    if (nl < 5u || name[nl - 4u] != '.' || (name[nl - 3u] | 0x20) != 'm' || (name[nl - 2u] | 0x20) != 'b' || (name[nl - 1u] | 0x20) != 'm') {
        return 0;
    }
    if (data == NULL) {
        return 1;
    }
    stem = nl - 4u;
    if (stem + 5u <= SEL_RP_COMPANION_NAME_MAX && k < max) {
        memcpy(out[k], name, stem);
        memcpy(out[k] + stem, ".MBK", 5u);
        k++;
    }
    if (size >= 0x148u && k < max) {
        for (i = 0; i < 8u && data[0x140u + i] != 0u && data[0x140u + i] != ' '; i++) {
            out[k][n++] = (char)data[0x140u + i];
        }
        if (n != 0u) {
            memcpy(out[k] + n, ".MBK", 5u);
            k++;
        }
    }
    return k;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "libkss");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "kss,mgs,mbm,bgm,opx,mpk");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10); /* above gme (0), which also claims .kss */
    set(&g_hooks[12], SEL_RP_CONTENT_COMPANIONS, SEL_TAG_FN(rp_content_companions));
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "libkss");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
