/* bbmp unitrk replayer (row I710m): a RENDER-kind adapter that plays MikMod's own module format (UNITRK, *.uni; what MikMod's converter MikCvt
 * made of XM, S3M, MOD and other modules) through libmikmod 3.3.14 itself (third_party/bbmp_unitrk, LGPL 2.1 or later), the way bbmp's engine
 * "unitrk" (csrc/pc/unitrk_engine.c) does: only the UNITRK loader and the no-sound driver are registered, the song is played once without
 * looping to measure it, and RP_START renders the whole song into memory at the host rate. libmikmod keeps its state in globals, so every
 * use is Init ... Exit within one call (open measures at 44100 Hz, start renders). */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <mikmod.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { kChunk = 4096, kMaxSeconds = 3600 };

typedef struct bbun_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    long duration_ms;
    char title[64];
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[6];
} bbun_state;

static int is_unitrk(const uint8_t* d, size_t n) {
    if (n >= 4u && memcmp(d, "UN0", 3) == 0 && d[3] >= '4' && d[3] <= '6') {
        return 1;
    }
    /* APUN is the later UNI of libmikmod 3.0's converter */
    return n >= 6u && memcmp(d, "APUN\01", 5) == 0;
}

static int ext_is_uni(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 4u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 'u' && (name[nl - 2u] | 0x20) == 'n' &&
           (name[nl - 1u] | 0x20) == 'i';
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (!is_unitrk(data, size)) {
        return SEL_PROBE_NO;
    }
    return ext_is_uni(name) ? SEL_PROBE_YES : SEL_PROBE_UNSURE;
}

static int mm_begin(uint32_t rate) {
    MikMod_RegisterDriver(&drv_nos);
    MikMod_RegisterLoader(&load_uni);
    md_mode = DMODE_16BITS | DMODE_STEREO | DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX | DMODE_INTERP;
    md_mixfreq = (UWORD)rate;
    md_reverb = 0;
    md_pansep = 128;
    return MikMod_Init("") == 0;
}

static MODULE* mm_load(const bbun_state* s) {
    MODULE* m = Player_LoadMem((const char*)s->data, (int)s->size, 128, 0);
    if (m != NULL) {
        m->loop = 0; /* a jump backwards ends the song */
        m->wrap = 0;
        m->fadeout = 0;
    }
    return m;
}

/* Plays the loaded module to its end (or `limit` frames); when `out` is not NULL, stores the frames there. Returns the frame count. */
static uint64_t mm_play(MODULE* m, int16_t** out, uint64_t limit) {
    static SWORD buf[kChunk * 2];
    uint64_t n = 0, cap = 0;
    int16_t* pcm = NULL;
    Player_Start(m);
    while (Player_Active() && n < limit) {
        ULONG got = VC_WriteBytes((SBYTE*)buf, kChunk * 4u) / 4u;
        if (got == 0u) {
            break;
        }
        if (out != NULL) {
            if (n + got > cap) {
                int16_t* grown;
                cap = cap == 0u ? 1u << 20 : cap * 2u;
                grown = (int16_t*)realloc(pcm, (size_t)cap * 4u);
                if (grown == NULL) {
                    free(pcm);
                    *out = NULL;
                    return 0;
                }
                pcm = grown;
            }
            memcpy(pcm + n * 2u, buf, (size_t)got * 4u);
        }
        n += got;
    }
    Player_Stop();
    if (out != NULL) {
        *out = pcm;
    }
    return n;
}

static void rp_close(void* state) {
    bbun_state* s = (bbun_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbun_state* s;
    MODULE* m;
    uint64_t frames;
    if (src->stream != NULL || src->data == NULL || !is_unitrk(src->data, src->size)) {
        return NULL;
    }
    s = (bbun_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)malloc(src->size);
    if (s->data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    if (!mm_begin(44100u)) {
        rp_close(s);
        return NULL;
    }
    m = mm_load(s);
    if (m == NULL) {
        MikMod_Exit();
        rp_close(s);
        return NULL;
    }
    if (m->songname != NULL) {
        strncpy(s->title, m->songname, sizeof s->title - 1u);
    }
    Player_Free(m);
    m = mm_load(s); /* a played module is spent: measure on a second copy */
    frames = m != NULL ? mm_play(m, NULL, (uint64_t)kMaxSeconds * 44100u) : 0u;
    if (m != NULL) {
        Player_Free(m);
    }
    MikMod_Exit();
    if (frames == 0u) {
        rp_close(s);
        return NULL;
    }
    s->duration_ms = (long)(frames * 1000u / 44100u);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbun_state* s = (bbun_state*)state;
    MODULE* m;
    int16_t* pcm = NULL;
    uint64_t n;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->frames = s->pos = 0;
    s->rate = rate;
    if (!mm_begin(rate)) {
        return 0;
    }
    m = mm_load(s);
    if (m == NULL) {
        MikMod_Exit();
        return 0;
    }
    n = mm_play(m, &pcm, (uint64_t)kMaxSeconds * rate);
    Player_Free(m);
    MikMod_Exit();
    if (pcm == NULL || n == 0u) {
        free(pcm);
        return 0;
    }
    s->pcm = pcm;
    s->frames = n;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbun_state* s = (bbun_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbun_state* s = (bbun_state*)state;
    if (s->pcm == NULL) {
        return 0;
    }
    s->pos = (uint64_t)ms * s->rate / 1000u;
    if (s->pos > s->frames) {
        s->pos = s->frames;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    bbun_state* s = (bbun_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "MikMod UNITRK";
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->duration_ms;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_unitrk");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "uni");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_unitrk");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
