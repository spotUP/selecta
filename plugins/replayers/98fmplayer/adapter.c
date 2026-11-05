/* 98fmplayer replayer (row I700, design 2026-10-26-online-sources section 2a): a RENDER-kind adapter over takamichih's
 * 98fmplayer (third_party/98fmplayer, BSD-2-Clause, plain C99) for PC-98 FM songs: PMD (.m, .m2, .mz) and FMP/PLAY6
 * (.ovi, .opi, .ozi). The driver is emulated on a YM2608 (libopna) at its fixed 55467 Hz; the adapter resamples by
 * straight lines to the host rate. A song plays until the driver reports its first loop (or stops). Companion PCM banks (I1127):
 * the driver names them (PMD .PPC and .PZI, FMP .PVI and .PZI) and the adapter reads them from the directory of
 * src->path, as the original player does and as the other render adapters do (mdxmini PDX, kss). Local songs have them
 * next to the file; library songs need the host cache to fetch them first (open: names derived from the song bytes).
 * A missing bank leaves that PCM silent. Not loaded: the OPNA drum ROM (proprietary, no free source: the YM2608 rhythm
 * samples are Yamaha's), so rhythm-part drums are silent. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "fmdriver/fmdriver.h"
#include "fmdriver/fmdriver_fmp.h"
#include "fmdriver/fmdriver_pmd.h"
#include "libopna/opna.h"
#include "libopna/opnaadpcm.h"
#include "libopna/opnatimer.h"

#include <stdio.h>
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
    struct {
        uint8_t* data;
        uint32_t size;
        int16_t* decode; /* PZI/PVI decode buffer the PPZ8 keeps pointing at */
    } bank[FMDRIVER_PCMCOUNT];
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
        int i;
        for (i = 0; i < FMDRIVER_PCMCOUNT; i++) {
            host->mem_free(host, s->bank[i].data);
            host->mem_free(host, s->bank[i].decode);
        }
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

/* Bank file extension for a driver PCM type, NULL when the slot has no file. */
static const char* bank_ext(const char* type) {
    if (strcmp(type, "PPC") == 0) return ".PPC";
    if (strcmp(type, "PVI") == 0) return ".PVI";
    if (strncmp(type, "PPZ", 3) == 0) return ".PZI";
    return NULL;
}

/* PPZ8 buffer of a PCM type: FMP "PPZ" and PMD "PPZ1" use buffer 0, PMD "PPZ2" buffer 1. */
static unsigned bank_ppz_buf(const char* type) {
    return type[3] == '2' ? 1u : 0u;
}

/* Reads `<dir of path><name><ext>` trying the case as written, lower and upper case (the archive's case is not the
 * driver's). Returns the host buffer or NULL. */
static uint8_t* read_bank(const sel_host* host, const char* path, const char* name, const char* ext, uint32_t* size) {
    char full[1100];
    size_t dir = 0, n = strlen(name), i, pass;
    const char* slash = strrchr(path, '/');
    *size = 0;
    if (slash != NULL) {
        dir = (size_t)(slash - path) + 1u;
    }
    if (n == 0u || n > 8u || dir + n + 5u > sizeof full || strchr(name, '/') != NULL) {
        return NULL;
    }
    for (pass = 0; pass < 3u; pass++) {
        FILE* f;
        long len;
        uint8_t* buf;
        memcpy(full, path, dir);
        for (i = 0; i < n; i++) {
            char c = name[i];
            full[dir + i] = pass == 1u ? (char)((c >= 'A' && c <= 'Z') ? c | 0x20 : c) : c;
        }
        for (i = 0; ext[i] != '\0'; i++) {
            full[dir + n + i] = pass == 1u ? (char)(ext[i] | 0x20) : ext[i];
        }
        full[dir + n + i] = '\0';
        f = fopen(full, "rb");
        if (f == NULL) {
            continue;
        }
        len = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : -1;
        buf = (len > 0 && len <= 0x1000000L && fseek(f, 0, SEEK_SET) == 0) ? (uint8_t*)host->mem_alloc(host, (size_t)len, SEL_MEM_FAST) : NULL;
        if (buf != NULL && fread(buf, 1, (size_t)len, f) != (size_t)len) {
            host->mem_free(host, buf);
            buf = NULL;
        }
        fclose(f);
        if (buf != NULL) {
            *size = (uint32_t)len;
            return buf;
        }
    }
    return NULL;
}

/* The one reader of the bank names a freshly initialised driver wants (work.pcmtype/pcmname), as file names with the
 * extension. Skips the slot load_banks skips (PPC in slot 3). Shared by load_banks (local lookup) and the
 * CONTENT_COMPANIONS hook (library cache). Returns how many names were written. */
static unsigned bank_names_from_work(const struct fmdriver_work* w, char out[][SEL_RP_COMPANION_NAME_MAX], unsigned max,
                                     int slot_of[FMDRIVER_PCMCOUNT]) {
    unsigned k = 0;
    int i;
    for (i = 0; i < FMDRIVER_PCMCOUNT && k < max; i++) {
        const char* ext = bank_ext(w->pcmtype[i]);
        char name[9];
        size_t n;
        memcpy(name, w->pcmname[i], 8);
        name[8] = '\0';
        for (n = strlen(name); n > 0u && (name[n - 1u] == ' ' || name[n - 1u] == '\0'); n--) {
            name[n - 1u] = '\0'; /* the driver pads names with spaces */
        }
        if (ext != NULL && n > 0u && !(i == 3 && strcmp(w->pcmtype[3], "PPC") == 0)) {
            snprintf(out[k], SEL_RP_COMPANION_NAME_MAX, "%s%s", name, ext);
            if (slot_of != NULL) {
                slot_of[k] = i;
            }
            k++;
        }
    }
    return k;
}

/* Reads the banks the freshly initialised driver names. */
static void load_banks(fmp98_state* s, const char* path) {
    char names[FMDRIVER_PCMCOUNT][SEL_RP_COMPANION_NAME_MAX];
    int slot[FMDRIVER_PCMCOUNT];
    unsigned k, n = bank_names_from_work(&s->work, names, FMDRIVER_PCMCOUNT, slot);
    for (k = 0; k < n; k++) {
        char* dot = strrchr(names[k], '.');
        char base[SEL_RP_COMPANION_NAME_MAX];
        size_t bl = (size_t)(dot - names[k]);
        memcpy(base, names[k], bl);
        base[bl] = '\0';
        s->bank[slot[k]].data = read_bank(s->host, path, base, dot, &s->bank[slot[k]].size);
    }
}

/* Feeds the loaded banks to the driver; runs after every driver init, because the chip and PPZ8 were just reset. */
static void apply_banks(fmp98_state* s) {
    int i;
    for (i = 0; i < FMDRIVER_PCMCOUNT; i++) {
        const char* type = s->work.pcmtype[i];
        if (s->bank[i].data == NULL) {
            continue;
        }
        if (strcmp(type, "PPC") == 0) {
            (void)pmd_ppc_load(&s->work, s->bank[i].data, s->bank[i].size);
        } else if (strcmp(type, "PVI") == 0) {
            (void)fmp_adpcm_load(&s->work, s->bank[i].data, s->bank[i].size);
        } else if (strncmp(type, "PPZ", 3) == 0) {
            uint32_t samples = ppz8_pzi_decodebuf_samples(s->bank[i].size);
            if (s->bank[i].decode == NULL && samples > 0u) {
                s->bank[i].decode = (int16_t*)s->host->mem_alloc(s->host, (size_t)samples * 2u, SEL_MEM_FAST);
            }
            if (s->bank[i].decode != NULL) {
                (void)ppz8_pzi_load(&s->ppz8, (uint8_t)bank_ppz_buf(type), s->bank[i].data, s->bank[i].size, s->bank[i].decode);
            }
        }
    }
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
    apply_banks(s);
    s->src_frames = 0;
    s->ended = 0;
    s->chunk_at = s->chunk_n = 0;
    s->pos = 0.0;
    memset(s->prev, 0, sizeof s->prev);
    memset(s->cur, 0, sizeof s->cur);
}

/* RP_CONTENT_COMPANIONS (I1127b): runs the same load + init as rp_open on a scratch state and reads the names the
 * driver wants. The scratch state is freed before return. */
static unsigned rp_content_companions(const char* name, const uint8_t* data, size_t size, char out[][SEL_RP_COMPANION_NAME_MAX],
                                      unsigned max, const sel_host* host) {
    fmp98_state* s;
    unsigned k = 0;
    int kind;
    if (!name_is_fmp(name) && !name_is_pmd(name)) {
        return 0;
    }
    if (data == NULL) {
        return 1;
    }
    if (size < 16u || size > 0xFFFFu || host == NULL) {
        return 0;
    }
    s = (fmp98_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return 0;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)host->mem_alloc(host, size, SEL_MEM_FAST);
    if (s->data != NULL) {
        memcpy(s->data, data, size);
        s->size = size;
        kind = load_driver(s, s->data, s->size, name);
        if (kind >= 0) {
            s->is_fmp = kind;
            reset_song(s);
            k = bank_names_from_work(&s->work, out, max, NULL);
        }
    }
    rp_close(s);
    return k;
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
    if (src->path != NULL) {
        load_banks(s, src->path);
        reset_song(s);
    }
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
    set(&g_hooks[12], SEL_RP_CONTENT_COMPANIONS, SEL_TAG_FN(rp_content_companions));
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "98fmplayer");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
