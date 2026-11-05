/* 98fmplayer replayer (row I700, design 2026-10-26-online-sources section 2a): a RENDER-kind adapter over takamichih's
 * 98fmplayer (third_party/98fmplayer, BSD-2-Clause, plain C99) for PC-98 FM songs: PMD (.m, .m2, .mz) and FMP/PLAY6
 * (.ovi, .opi, .ozi). The driver is emulated on a YM2608 (libopna) at its fixed 55467 Hz; the adapter resamples by
 * straight lines to the host rate. A song plays until the driver reports its first loop (or stops). Not loaded: the
 * companion PCM banks (.PPC/.PVI/.PZI, found next to the song by the original player) and the OPNA drum ROM
 * (proprietary), so ADPCM and rhythm-part drums are silent. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "fmdriver/fmdriver.h"
#include "fmdriver/fmdriver_fmp.h"
#include "fmdriver/fmdriver_pmd.h"
#include "libopna/opna.h"
#include "libopna/opnaadpcm.h"
#include "libopna/opnatimer.h"

#include <stdlib.h>
#include <string.h>

#define FMP98_SRATE 55467u
#define FMP98_PPZ8_MIX 0xa000u
#define FMP98_CHUNK 512u
#define FMP98_MAX_FRAMES (FMP98_SRATE * 1200u) /* a song that never loops or stops ends after 20 minutes */
#define FMP98_SKIP_CHUNK 1024u

typedef struct fmp98_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int is_fmp;
    union {
        struct driver_pmd pmd;
        struct driver_fmp fmp;
    } drv;
    struct fmdriver_work work;
    struct opna opna;
    struct opna_timer timer;
    struct ppz8 ppz8;
    char adpcm_ram[OPNA_ADPCM_RAM_SIZE];
    int started;
    int ended;
    uint32_t rate;
    uint64_t src_frames;
    /* resampler: straight lines between source frames */
    double step, pos;
    int16_t prev[2], cur[2];
    int16_t chunk[FMP98_CHUNK * 2];
    unsigned chunk_at, chunk_n;
    char title[256];
    sel_tag meta[8];
    char format[8];
} fmp98_state;

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

static int name_is_fmp(const char* name) {
    return ext_is(name, "ovi") || ext_is(name, "opi") || ext_is(name, "ozi");
}

static int name_is_pmd(const char* name) {
    return ext_is(name, "m") || ext_is(name, "m2") || ext_is(name, "mz");
}

/* Loads `data` (kept by the driver, not copied) into the zeroed driver slot; returns 1 for FMP, 0 for PMD, -1 on failure. */
static int load_driver(fmp98_state* s, uint8_t* data, size_t size, const char* name) {
    int fmp_first = name_is_fmp(name);
    int pass;
    if (size < 16u || size > 0xFFFFu) {
        return -1;
    }
    for (pass = 0; pass < 2; pass++) {
        int want_fmp = (pass == 0) ? fmp_first : !fmp_first;
        memset(&s->drv, 0, sizeof s->drv);
        if (want_fmp ? fmp_load(&s->drv.fmp, data, (uint16_t)size) : pmd_load(&s->drv.pmd, data, (uint16_t)size)) {
            return want_fmp;
        }
    }
    return -1;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    fmp98_state* s;
    int r;
    uint8_t* copy;
    if (!name_is_fmp(name) && !name_is_pmd(name)) {
        return SEL_PROBE_NO; /* PMD has no magic: the extension is part of the test */
    }
    s = (fmp98_state*)calloc(1, sizeof *s);
    copy = (uint8_t*)malloc(size > 0u ? size : 1u);
    if (s == NULL || copy == NULL) {
        free(s);
        free(copy);
        return SEL_PROBE_NO;
    }
    memcpy(copy, data, size);
    r = load_driver(s, copy, size, name);
    free(s);
    free(copy);
    return r >= 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    fmp98_state* s = (fmp98_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

/* CP932 title: keep printable ASCII, drop ANSI escape sequences (ESC [ ... letter) and everything above 0x7E. */
static void take_title(fmp98_state* s) {
    const char* t = s->work.get_comment != NULL ? s->work.get_comment(&s->work, 0) : NULL;
    size_t n = 0, i;
    if (t == NULL) {
        return;
    }
    for (i = 0; t[i] != '\0' && n + 1u < sizeof s->title; i++) {
        unsigned char c = (unsigned char)t[i];
        if (c == 0x1Bu) {
            if (t[i + 1] == '[') {
                i += 2u;
                while (t[i] != '\0' && !((t[i] >= 'A' && t[i] <= 'Z') || (t[i] >= 'a' && t[i] <= 'z'))) {
                    i++;
                }
                if (t[i] == '\0') {
                    break;
                }
            }
            continue;
        }
        if (c >= 0x20u && c < 0x7Fu) {
            s->title[n++] = (char)c;
        }
    }
    while (n > 0u && s->title[n - 1u] == ' ') {
        n--;
    }
    s->title[n] = '\0';
}

static void opna_writereg_cb(struct fmdriver_work* w, unsigned addr, unsigned data) {
    opna_timer_writereg((struct opna_timer*)w->opna, addr, data);
}

static unsigned opna_readreg_cb(struct fmdriver_work* w, unsigned addr) {
    struct opna_timer* t = (struct opna_timer*)w->opna;
    return opna_readreg(t->opna, addr);
}

static uint8_t opna_status_cb(struct fmdriver_work* w, bool a1) {
    uint8_t st = opna_timer_status((struct opna_timer*)w->opna);
    return a1 ? st : (uint8_t)(st & 0x83u);
}

static void opna_int_cb(void* user) {
    struct fmdriver_work* w = (struct fmdriver_work*)user;
    w->driver_opna_interrupt(w);
}

static void opna_mix_cb(void* user, int16_t* buf, unsigned samples) {
    ppz8_mix((struct ppz8*)user, buf, samples);
}

/* Resets the chip and starts the driver from the top (mirrors fmplayer_init_work_opna + fmplayer_file_load). */
static void reset_song(fmp98_state* s) {
    opna_reset(&s->opna);
    opna_adpcm_set_ram_256k(&s->opna.adpcm, s->adpcm_ram);
    opna_timer_reset(&s->timer, &s->opna);
    ppz8_init(&s->ppz8, FMP98_SRATE, FMP98_PPZ8_MIX);
    memset(&s->work, 0, sizeof s->work);
    s->work.opna_writereg = opna_writereg_cb;
    s->work.opna_readreg = opna_readreg_cb;
    s->work.opna_status = opna_status_cb;
    s->work.opna = &s->timer;
    s->work.ppz8 = &s->ppz8;
    s->work.ppz8_functbl = &ppz8_functbl;
    opna_timer_set_int_callback(&s->timer, opna_int_cb, &s->work);
    opna_timer_set_mix_callback(&s->timer, opna_mix_cb, &s->ppz8);
    if (s->is_fmp) {
        fmp_init(&s->work, &s->drv.fmp);
    } else {
        pmd_init(&s->work, &s->drv.pmd);
    }
    s->src_frames = 0;
    s->ended = 0;
    s->chunk_at = s->chunk_n = 0;
    s->pos = 0.0;
    memset(s->prev, 0, sizeof s->prev);
    memset(s->cur, 0, sizeof s->cur);
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    fmp98_state* s;
    int kind;
    if (src->stream != NULL || src->data == NULL || src->size < 16u || src->size > 0xFFFFu) {
        return NULL;
    }
    s = (fmp98_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
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
    kind = load_driver(s, s->data, s->size, src->path);
    if (kind < 0) {
        rp_close(s);
        return NULL;
    }
    s->is_fmp = kind;
    reset_song(s);
    take_title(s);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    fmp98_state* s = (fmp98_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    s->step = (double)FMP98_SRATE / (double)rate;
    reset_song(s);
    s->started = 1;
    return 1;
}

/* Next source frame (stereo) from the chip. */
static void next_src(fmp98_state* s, int16_t out[2]) {
    if (s->chunk_at >= s->chunk_n) {
        memset(s->chunk, 0, sizeof s->chunk);
        opna_timer_mix(&s->timer, s->chunk, FMP98_CHUNK);
        s->chunk_at = 0;
        s->chunk_n = FMP98_CHUNK;
    }
    out[0] = s->chunk[s->chunk_at * 2u];
    out[1] = s->chunk[s->chunk_at * 2u + 1u];
    s->chunk_at++;
    s->src_frames++;
    if (s->work.loop_cnt >= 1u || !s->work.playing || s->src_frames >= FMP98_MAX_FRAMES) {
        s->ended = 1;
    }
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    fmp98_state* s = (fmp98_state*)state;
    uint32_t i;
    if (!s->started || s->ended) {
        return 0;
    }
    for (i = 0; i < frames; i++) {
        while (s->pos >= 1.0) {
            s->prev[0] = s->cur[0];
            s->prev[1] = s->cur[1];
            next_src(s, s->cur);
            s->pos -= 1.0;
        }
        out[i * 2u] = (int16_t)(s->prev[0] + (int)((s->cur[0] - s->prev[0]) * s->pos));
        out[i * 2u + 1u] = (int16_t)(s->prev[1] + (int)((s->cur[1] - s->prev[1]) * s->pos));
        s->pos += s->step;
    }
    return frames;
}

static int rp_seek(void* state, uint32_t ms) {
    fmp98_state* s = (fmp98_state*)state;
    uint64_t left = (uint64_t)ms * FMP98_SRATE / 1000u;
    int16_t dummy[2];
    if (!s->started) {
        return 0;
    }
    /* No random access: restart the song and run the emulation forward, discarding the output. */
    reset_song(s);
    while (left > 0u && !s->ended) {
        next_src(s, dummy);
        left--;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    fmp98_state* s = (fmp98_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    strcpy(s->format, s->is_fmp ? "FMP" : "PMD");
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "98fmplayer");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "m,m2,mz,ovi,opi,ozi");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "98fmplayer");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
