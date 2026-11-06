/* bbmp jxs replayer (row I710i): a RENDER-kind adapter over bbmp's csrc/synth JayTrax engine (third_party/bbmp_jxs, GPL-3.0-or-later,
 * a clean-room replayer). The engine renders stereo floats at 44100 Hz and keeps its state in one global, so a song is opened
 * for its info, closed, and opened again at RP_START where the whole song is rendered and (when the host rate differs)
 * resampled linearly. The engine's open() validates the file, so a non-JayTrax file fails there. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct synth_info {
    int rate;
    int tracks;
    long duration_ms;
    char title[256];
    char author[256];
    char type[64];
    char comment[4096];
    char error[256];
    char names[256][68];
    int n_names;
} synth_info;

int synthw_jxs_open(const char* path, const uint8_t* data, size_t size, int track, synth_info* info);
int synthw_jxs_render(float* out, int frames);
void synthw_jxs_close(void);

enum { kChunk = 4096, kJxsRate = 44100, kJxsMaxFrames = 3600 * kJxsRate };

typedef struct bbjxs_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int tracks;
    long duration_ms;
    char title[256];
    char author[256];
    char type[64];
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} bbjxs_state;

static int ext_is_jxs(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 5u && name[nl - 4u] == '.' && (name[nl - 3u] | 0x20) == 'j' && (name[nl - 2u] | 0x20) == 'x' &&
           (name[nl - 1u] | 0x20) == 's';
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    uint32_t v;
    if (!ext_is_jxs(name) || size < 52u) {
        return SEL_PROBE_NO;
    }
    /* The file starts with a version word, 3456 or 3457; the engine's open() validates the rest of the header. */
    v = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
    return v == 3456u || v == 3457u ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbjxs_state* s = (bbjxs_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbjxs_state* s;
    synth_info* si;
    int ok;
    if (src->stream != NULL || src->data == NULL || src->size < 52u) {
        return NULL;
    }
    s = (bbjxs_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    si = (synth_info*)calloc(1, sizeof *si);
    if (s == NULL || si == NULL) {
        free(si);
        if (s != NULL) {
            host->mem_free(host, s);
        }
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)malloc(src->size);
    if (s->data == NULL) {
        free(si);
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    ok = synthw_jxs_open("", s->data, s->size, 0, si);
    if (!ok) {
        free(si);
        free(s->data);
        host->mem_free(host, s);
        return NULL;
    }
    synthw_jxs_close();
    s->tracks = si->tracks > 0 ? si->tracks : 1;
    s->duration_ms = si->duration_ms;
    memcpy(s->title, si->title, sizeof s->title);
    memcpy(s->author, si->author, sizeof s->author);
    memcpy(s->type, si->type, sizeof s->type);
    free(si);
    info->voice_count = 0;
    info->subsong_count = (uint32_t)s->tracks;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbjxs_state* s = (bbjxs_state*)state;
    synth_info* si;
    float* buf;
    int16_t* native = NULL;
    uint64_t n = 0, cap = 0;
    int ok;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= (uint32_t)s->tracks) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->frames = s->pos = 0;
    s->rate = rate;
    si = (synth_info*)calloc(1, sizeof *si);
    buf = (float*)malloc(sizeof(float) * 2u * kChunk);
    if (si == NULL || buf == NULL) {
        free(si);
        free(buf);
        return 0;
    }
    ok = synthw_jxs_open("", s->data, s->size, (int)subsong, si);
    free(si);
    if (!ok) {
        free(buf);
        return 0;
    }
    for (;;) {
        int got = synthw_jxs_render(buf, kChunk), i;
        if (got > 0) {
            if (n + (uint64_t)got > cap) {
                int16_t* grown;
                cap = cap == 0u ? 1u << 20 : cap * 2u;
                grown = (int16_t*)realloc(native, (size_t)cap * 4u);
                if (grown == NULL) {
                    ok = 0;
                    break;
                }
                native = grown;
            }
            for (i = 0; i < got * 2; i++) {
                float v = buf[i] * 32767.0f;
                native[n * 2u + (uint64_t)i] = (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
            }
            n += (uint64_t)got;
        }
        if (got < kChunk || n >= (uint64_t)kJxsMaxFrames) {
            break;
        }
    }
    synthw_jxs_close();
    free(buf);
    if (!ok || n == 0u) {
        free(native);
        return 0;
    }
    if (rate == (uint32_t)kJxsRate) {
        s->pcm = native;
        s->frames = n;
    } else {
        uint64_t out = n * rate / kJxsRate, k;
        int16_t* r = (int16_t*)malloc((size_t)out * 4u + 4u);
        if (r == NULL) {
            free(native);
            return 0;
        }
        for (k = 0; k < out; k++) {
            double p = (double)k * kJxsRate / rate;
            uint64_t i0 = (uint64_t)p, i1 = i0 + 1u < n ? i0 + 1u : i0;
            double f = p - (double)i0;
            int c;
            for (c = 0; c < 2; c++) {
                r[k * 2u + (uint64_t)c] =
                    (int16_t)((double)native[i0 * 2u + (uint64_t)c] * (1.0 - f) + (double)native[i1 * 2u + (uint64_t)c] * f);
            }
        }
        free(native);
        s->pcm = r;
        s->frames = out;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbjxs_state* s = (bbjxs_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbjxs_state* s = (bbjxs_state*)state;
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
    bbjxs_state* s = (bbjxs_state*)state;
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
    s->meta[n++].value = (uintptr_t)(s->type[0] != '\0' ? s->type : "JayTrax");
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_jxs");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "jxs");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_jxs");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
