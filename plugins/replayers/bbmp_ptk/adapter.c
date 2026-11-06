/* bbmp ptk replayer (row I710q): a RENDER-kind adapter that plays NoiseTrekker, NoiseTrekker 2, ProTrekkr and ProTrekkr 2.0 songs (*.ntk,
 * *.ptk) through ProTrekkr's own replay routine (third_party/bbmp_ptk, BSD 2-clause, taken from rePlayer 0.22.14), the engine bbmp uses for
 * them. shim.cpp wraps the replayer (global state, one song at a time); open renders the whole song once at the replayer's fixed 44100 Hz
 * (the song ends when the replayer says so, capped at kMaxSeconds), and RP_START hands it out, resampled linearly to the host rate. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int ptks_probe(const uint8_t* d, size_t n);
int ptks_open(const uint8_t* data, size_t size, char* title, size_t title_size, int* ntk);
int ptks_render(float* out, int frames);
void ptks_close(void);

enum { kChunk = 4096, kMaxSeconds = 3600, kBaseRate = 44100 };

typedef struct bbpk_state {
    const sel_host* host;
    int16_t* base; /* the song at kBaseRate */
    uint64_t base_frames;
    int16_t* pcm; /* at the rate of RP_START */
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    long duration_ms;
    char title[64];
    const char* type;
    sel_tag meta[6];
} bbpk_state;

static int ext_is_ptk(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 4u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 'p' && (name[nl - 2u] | 0x20) == 't' &&
           (name[nl - 1u] | 0x20) == 'k';
}

static int ext_is_ntk(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 4u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 'n' && (name[nl - 2u] | 0x20) == 't' &&
           (name[nl - 1u] | 0x20) == 'k';
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (!ptks_probe(data, size)) {
        return SEL_PROBE_NO;
    }
    return ext_is_ptk(name) || ext_is_ntk(name) ? SEL_PROBE_YES : SEL_PROBE_UNSURE;
}

static int16_t clip16(float v) {
    v *= 32767.0f;
    return (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
}

static void rp_close(void* state) {
    bbpk_state* s = (bbpk_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->base);
        free(s->pcm);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    static float buf[kChunk * 2];
    bbpk_state* s;
    uint64_t n = 0, cap = 0;
    int ntk = 0;
    if (src->stream != NULL || src->data == NULL || !ptks_probe(src->data, src->size)) {
        return NULL;
    }
    s = (bbpk_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    if (!ptks_open(src->data, src->size, s->title, sizeof s->title, &ntk)) {
        rp_close(s);
        return NULL;
    }
    s->type = ntk ? "NoiseTrekker" : "ProTrekkr";
    for (;;) {
        int got = ptks_render(buf, kChunk);
        if (got <= 0) {
            break;
        }
        if (n + (uint64_t)got > cap) {
            int16_t* grown;
            cap = cap == 0u ? 1u << 20 : cap * 2u;
            grown = (int16_t*)realloc(s->base, (size_t)cap * 4u);
            if (grown == NULL) {
                ptks_close();
                rp_close(s);
                return NULL;
            }
            s->base = grown;
        }
        for (int i = 0; i < got * 2; i++) {
            s->base[n * 2u + (uint64_t)i] = clip16(buf[i]);
        }
        n += (uint64_t)got;
        if (got < kChunk || n >= (uint64_t)kMaxSeconds * kBaseRate) {
            break;
        }
    }
    ptks_close();
    if (n == 0u) {
        rp_close(s);
        return NULL;
    }
    s->base_frames = n;
    s->duration_ms = (long)(n * 1000u / kBaseRate);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbpk_state* s = (bbpk_state*)state;
    uint64_t n, i;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u || s->base == NULL) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (rate == (uint32_t)kBaseRate) {
        s->pcm = s->base;
        s->base = NULL;
        s->frames = s->base_frames;
        s->base_frames = 0;
        return 1;
    }
    n = s->base_frames * rate / kBaseRate;
    s->pcm = (int16_t*)malloc((size_t)n * 4u);
    if (s->pcm == NULL) {
        return 0;
    }
    for (i = 0; i < n; i++) { /* linear interpolation */
        uint64_t num = i * kBaseRate;
        uint64_t a = num / rate, b = a + 1u < s->base_frames ? a + 1u : a;
        uint32_t f = (uint32_t)((num % rate) * 256u / rate);
        for (int c = 0; c < 2; c++) {
            int x = s->base[a * 2u + (uint64_t)c], y = s->base[b * 2u + (uint64_t)c];
            s->pcm[i * 2u + (uint64_t)c] = (int16_t)(x + (int)(((int)(y - x) * (int)f) / 256));
        }
    }
    s->frames = n;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbpk_state* s = (bbpk_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbpk_state* s = (bbpk_state*)state;
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
    bbpk_state* s = (bbpk_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->type;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_ptk");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "ntk,ptk");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_ptk");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
