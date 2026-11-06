/* bbmp mxm replayer (row I710f): a RENDER-kind adapter over bbmp's csrc/pc Cubic Tiny XM engine (third_party/bbmp_mxm, GPL-3.0-or-later,
 * a clean-room converter): .mxm is turned into an XM module in memory, which Selecta's vendored libopenmpt plays (the engine's own
 * openmpt.cpp wrapper). The engine keeps its state in one global, so a song is opened for its info, closed, and opened again at
 * RP_START where the whole song is rendered at the host rate (libopenmpt resamples itself). The MXM format has no companion file. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct pc_info {
    int rate;
    int tracks;
    long duration_ms;
    char title[256];
    char author[256];
    char type[64];
    char comment[8192];
    char error[256];
    char names[512][68];
    int n_names;
} pc_info;

int pcw_mxm_open(const char* path, const uint8_t* data, size_t size, int track, pc_info* info);
int pcw_mxm_render(float* out, int frames);
void pcw_mxm_close(void);

enum { kChunk = 4096, kMaxSeconds = 3600, kFixedPart = 1872 };

typedef struct bbmxm_state {
    const sel_host* host;
    uint8_t* data;
    char path[1024];
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
} bbmxm_state;

static int ext_ci(const char* name, const char* ext) {
    size_t nl = name != NULL ? strlen(name) : 0u, el = strlen(ext), i;
    if (nl <= el + 1u || name[nl - el - 1u] != '.') {
        return 0;
    }
    for (i = 0; i < el; i++) {
        if ((name[nl - el + i] | 0x20) != ext[i]) {
            return 0;
        }
    }
    return 1;
}

/* Cubic Tiny XM: "MXM\0" and the fixed 1872-byte part. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (ext_ci(name, "mxm") && size >= (size_t)kFixedPart && memcmp(data, "MXM\0", 4) == 0) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbmxm_state* s = (bbmxm_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbmxm_state* s;
    pc_info* si;
    if (src->stream != NULL || src->data == NULL || src->path == NULL || strlen(src->path) >= sizeof((bbmxm_state*)0)->path ||
        src->size < (size_t)kFixedPart) {
        return NULL;
    }
    s = (bbmxm_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    si = (pc_info*)calloc(1, sizeof *si);
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
    strcpy(s->path, src->path);
    si->rate = 48000;
    if (!pcw_mxm_open(s->path, s->data, s->size, 0, si)) {
        free(si);
        free(s->data);
        host->mem_free(host, s);
        return NULL;
    }
    pcw_mxm_close();
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
    bbmxm_state* s = (bbmxm_state*)state;
    pc_info* si;
    float* buf;
    int16_t* pcm = NULL;
    uint64_t n = 0, cap = 0, max_frames = (uint64_t)kMaxSeconds * rate;
    int ok;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= (uint32_t)s->tracks) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->frames = s->pos = 0;
    s->rate = rate;
    si = (pc_info*)calloc(1, sizeof *si);
    buf = (float*)malloc(sizeof(float) * 2u * kChunk);
    if (si == NULL || buf == NULL) {
        free(si);
        free(buf);
        return 0;
    }
    si->rate = (int)rate;
    ok = pcw_mxm_open(s->path, s->data, s->size, (int)subsong, si);
    free(si);
    if (!ok) {
        free(buf);
        return 0;
    }
    for (;;) {
        int got = pcw_mxm_render(buf, kChunk), i;
        if (got > 0) {
            if (n + (uint64_t)got > cap) {
                int16_t* grown;
                cap = cap == 0u ? 1u << 20 : cap * 2u;
                grown = (int16_t*)realloc(pcm, (size_t)cap * 4u);
                if (grown == NULL) {
                    ok = 0;
                    break;
                }
                pcm = grown;
            }
            for (i = 0; i < got * 2; i++) {
                float v = buf[i] * 32767.0f;
                pcm[n * 2u + (uint64_t)i] = (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
            }
            n += (uint64_t)got;
        }
        if (got < kChunk || n >= max_frames) {
            break;
        }
    }
    pcw_mxm_close();
    free(buf);
    if (!ok || n == 0u) {
        free(pcm);
        return 0;
    }
    s->pcm = pcm;
    s->frames = n;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbmxm_state* s = (bbmxm_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbmxm_state* s = (bbmxm_state*)state;
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
    bbmxm_state* s = (bbmxm_state*)state;
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
    s->meta[n++].value = (uintptr_t)(s->type[0] != '\0' ? s->type : "Cubic Tiny XM");
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_mxm");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "mxm");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_mxm");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
