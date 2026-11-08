/* bbmp PlayerPRO replayer (row I710aq): a RENDER-kind adapter over bbmp's csrc/pc/playerpro_engine.c (third_party/bbmp_playerpro, GPL-3.0-or-later,
 * on PlayerPROCore, PlayerPRO's public-domain driver). PlayerPRO MAD songs (.mad): MADK, MAD-F, MAD-G, MADH, MADI. The extension .mad is shared with adplug
 * (Mlat Adlib Tracker, magic "MAD+"), so the probe claims only the magic "MAD" plus one of F G H I K and a size of at least 100 bytes; the engine then refuses
 * damaged files (open returns NULL). The engine reads the song by path: the adapter writes it to a temporary file for the open and removes it at once. It
 * renders at up to 48000 Hz and keeps one song in static variables: a second open while one is open is refused. The song is rendered whole at RP_START and
 * resampled linearly when the host rate differs. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "engine.h"

#define PP_RATE 44100u

typedef struct bbpp_state {
    const sel_host* host;
    float* native;      /* the engine's whole song, 44100 Hz stereo */
    uint64_t native_frames;
    int16_t* pcm;       /* the same at the host rate */
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    uint32_t duration_ms;
    char type[64];
    char title[256];
    sel_tag meta[6];
} bbpp_state;

static int g_open; /* the engine has one song in static storage */

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return size >= 100u && memcmp(data, "MAD", 3) == 0 && data[3] != 0 && strchr("FGHIK", data[3]) != NULL ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbpp_state* s = (bbpp_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->native);
        pcw_playerpro_close();
        g_open = 0;
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbpp_state* s;
    pc_info* pi;
    int ok;
    if (src->stream != NULL || src->data == NULL || g_open) {
        return NULL;
    }
    pi = (pc_info*)calloc(1, sizeof *pi);
    if (pi == NULL) {
        return NULL;
    }
    pi->rate = (int)PP_RATE;
    {
        const char* dir = getenv("TMPDIR");
        char path[1024];
        int fd;
        size_t put = 0;
        snprintf(path, sizeof path, "%s/selecta_pp_XXXXXX.mad", dir != NULL && dir[0] != '\0' ? dir : "/tmp");
        fd = mkstemps(path, 4);
        if (fd < 0) {
            free(pi);
            return NULL;
        }
        while (put < (size_t)src->size) {
            ssize_t w = write(fd, src->data + put, (size_t)src->size - put);
            if (w <= 0) {
                break;
            }
            put += (size_t)w;
        }
        close(fd);
        ok = put == (size_t)src->size && pcw_playerpro_open(path, src->data, (size_t)src->size, 0, pi);
        unlink(path);
    }
    if (!ok) {
        free(pi);
        return NULL;
    }
    s = (bbpp_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        pcw_playerpro_close();
        free(pi);
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    snprintf(s->type, sizeof s->type, "%s", pi->type);
    snprintf(s->title, sizeof s->title, "%s", pi->title);
    s->duration_ms = pi->duration_ms > 0 ? (uint32_t)pi->duration_ms : 0u;
    g_open = 1;
    free(pi);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

/* The engine's song is stateful and played once: the first RP_START renders it; a restart re-opens nothing and replays the
 * stored PCM. */
static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbpp_state* s = (bbpp_state*)state;
    uint64_t cap = (uint64_t)PP_RATE * 3600u;
    uint64_t n = 0;
    float* buf;
    float* all = NULL;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    s->pos = 0;
    if (s->pcm != NULL && s->rate == rate) {
        return 1;
    }
    if (s->native == NULL) {
        /* first start: render the engine's 44100 Hz stream to the end */
        uint64_t have = 0, room = (uint64_t)PP_RATE * 60u;
        buf = (float*)malloc(sizeof(float) * 2u * 4096u);
        all = (float*)malloc(sizeof(float) * 2u * room);
        if (buf == NULL || all == NULL) {
            free(buf);
            free(all);
            return 0;
        }
        for (;;) {
            int got = pcw_playerpro_render(buf, 4096);
            if (got <= 0) {
                break;
            }
            if (have + (uint64_t)got > room) {
                float* bigger;
                room *= 2u;
                bigger = (float*)realloc(all, sizeof(float) * 2u * room);
                if (bigger == NULL) {
                    break;
                }
                all = bigger;
            }
            memcpy(all + have * 2u, buf, sizeof(float) * 2u * (size_t)got);
            have += (uint64_t)got;
            if (have >= cap || got < 4096) {
                break;
            }
        }
        free(buf);
        n = have;
        if (n == 0u) {
            free(all);
            return 0;
        }
        s->native = all;
        s->native_frames = n;
    }
    {
        const float* src = s->native;
        uint64_t out_frames = s->native_frames * rate / PP_RATE;
        int16_t* out;
        uint64_t i;
        if (out_frames == 0u) {
            return 0;
        }
        out = (int16_t*)malloc((size_t)out_frames * 4u + 4u);
        if (out == NULL) {
            return 0;
        }
        for (i = 0; i < out_frames; i++) {
            double pos = (double)i * PP_RATE / rate;
            uint64_t i0 = (uint64_t)pos;
            uint64_t i1 = i0 + 1u < s->native_frames ? i0 + 1u : i0;
            float f = (float)(pos - (double)i0);
            int c;
            for (c = 0; c < 2; c++) {
                float v = src[i0 * 2u + (uint64_t)c] * (1.0f - f) + src[i1 * 2u + (uint64_t)c] * f;
                int q = (int)(v * 32767.0f);
                out[i * 2u + (uint64_t)c] = (int16_t)(q > 32767 ? 32767 : (q < -32768 ? -32768 : q));
            }
        }
        free(s->pcm);
        s->pcm = out;
        s->frames = out_frames;
        s->rate = rate;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbpp_state* s = (bbpp_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbpp_state* s = (bbpp_state*)state;
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
    bbpp_state* s = (bbpp_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) s->type;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_playerpro");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "mad");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_playerpro");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
