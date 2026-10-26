/* dr_flac stream decoder (design doc "Format coverage rule", I436): a RENDER-kind replayer over a FLAC byte
 * stream held in memory. dr_flac decodes to interleaved int16 at the file's own rate; the adapter folds it to
 * stereo, resamples to the output rate (stream_pcm.h) and reports album and artist from the Vorbis comments
 * (stream/flac_meta.h, the same reader the library scan uses for the embedded cover). Integer decoding only.
 * Desktop first: live http streams and file-backed byte streams are not claimed yet. */
#include <dr_flac.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "stream/flac_meta.h"
#include "stream/stream_pcm.h"

#include <stdlib.h>
#include <string.h>

#define FLAC_MAX_CH 8u
#define FLAC_TAGLEN 128u

typedef struct flac_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    drflac* fl;
    uint32_t hz, channels;
    uint64_t total_frames;
    uint32_t rate;
    sel_stream_pcm pcm;
    char album[FLAC_TAGLEN], artist[FLAC_TAGLEN];
    sel_tag meta[8];
} flac_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return size >= 8u && memcmp(data, "fLaC", 4) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    flac_state* s;
    sel_cover cover;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (flac_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->fl = drflac_open_memory(src->data, src->size, NULL);
    if (s->fl == NULL || s->fl->channels == 0u || s->fl->channels > FLAC_MAX_CH) {
        if (s->fl != NULL) {
            drflac_close(s->fl);
        }
        host->mem_free(host, s);
        return NULL;
    }
    s->hz = s->fl->sampleRate;
    s->channels = s->fl->channels;
    s->total_frames = s->fl->totalPCMFrameCount;
    (void)sel_flac_meta(src->data, src->size, 0u, &cover, s->album, s->artist, FLAC_TAGLEN);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    flac_state* s = (flac_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        drflac_close(s->fl);
        host->mem_free(host, s);
    }
}

/* Source frames as stereo: mono duplicated, more than two channels keep the first two. */
static uint32_t pull(void* ctx, int16_t* out, uint32_t frames) {
    flac_state* s = (flac_state*)ctx;
    int16_t raw[SEL_STREAM_CHUNK * FLAC_MAX_CH];
    uint32_t n = 0, i, ch = s->channels;
    while (n < frames) {
        uint32_t want = frames - n, got;
        if (want > SEL_STREAM_CHUNK) {
            want = SEL_STREAM_CHUNK;
        }
        got = (uint32_t)drflac_read_pcm_frames_s16(s->fl, want, raw);
        if (got == 0u) {
            break;
        }
        for (i = 0; i < got; i++) {
            out[(n + i) * 2u] = raw[i * ch];
            out[(n + i) * 2u + 1u] = raw[i * ch + (ch > 1u ? 1u : 0u)];
        }
        n += got;
    }
    return n;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    flac_state* s = (flac_state*)state;
    (void)subsong;
    (void)voices;
    if (rate == 0u) {
        return 0;
    }
    s->rate = rate;
    (void)drflac_seek_to_pcm_frame(s->fl, 0);
    sel_stream_pcm_reset(&s->pcm, pull, s, s->hz, rate);
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    flac_state* s = (flac_state*)state;
    return sel_stream_pcm_read(&s->pcm, out, frames);
}

static int rp_seek(void* state, uint32_t ms) {
    flac_state* s = (flac_state*)state;
    if (!drflac_seek_to_pcm_frame(s->fl, (drflac_uint64)ms * s->hz / 1000u)) {
        return 0;
    }
    sel_stream_pcm_reset(&s->pcm, pull, s, s->hz, s->rate);
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    flac_state* s = (flac_state*)state;
    uint32_t n = 0;
    if (s->artist[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->artist;
    }
    if (s->album[0] != '\0') {
        s->meta[n].tag = SEL_META_RELEASE;
        s->meta[n++].value = (uintptr_t)s->album;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "FLAC";
    s->meta[n].tag = SEL_META_CHANNELS;
    s->meta[n++].value = (uintptr_t)s->channels;
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)(s->total_frames * 1000u / s->hz);
    s->meta[n].tag = SEL_META_SIZE;
    s->meta[n++].value = (uintptr_t)s->size;
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "dr_flac");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "flac");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "dr_flac");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
