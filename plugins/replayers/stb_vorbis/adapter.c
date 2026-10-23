/* stb_vorbis stream decoder (design doc section 4, D-M3-8): a RENDER-kind replayer over an Ogg Vorbis byte
 * stream. stb_vorbis decodes from the source bytes (the declarations come from the header-only view of
 * stb_vorbis.c, the implementation is the selecta_stb_vorbis library). Float decoding, so the plugin carries
 * SEL_RP_NEEDS_FLOAT and the M2 machine filter hides it on a machine without an FPU. */
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#include <stb_vorbis.c>

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "../stream/stream_pcm.h"

#include <stdlib.h>
#include <string.h>

#define META_LEN 128u

typedef struct vorbis_state {
    const sel_host* host;
    stb_vorbis* v;
    size_t size;
    uint32_t hz, rate;
    uint32_t channels, total_frames;
    sel_stream_pcm pcm;
    char title[META_LEN], author[META_LEN];
    sel_tag meta[7];
} vorbis_state;

/* "OggS", then the Vorbis identification packet (type 1, "vorbis") in the first page. Ogg also carries Opus
 * and FLAC, which stb_vorbis cannot decode. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i;
    (void)name;
    if (size < 36u || memcmp(data, "OggS", 4) != 0) {
        return SEL_PROBE_NO;
    }
    for (i = 26u; i + 7u <= 64u && i + 7u <= size; i++) {
        if (data[i] == 1u && memcmp(data + i + 1u, "vorbis", 6) == 0) {
            return SEL_PROBE_YES;
        }
    }
    return SEL_PROBE_NO;
}

/* Vorbis comment "KEY=value" with a case-insensitive key; copies the value into dst. */
static void comment(const stb_vorbis_comment* c, const char* key, char* dst) {
    size_t kl = strlen(key);
    int i;
    dst[0] = '\0';
    for (i = 0; i < c->comment_list_length; i++) {
        const char* e = c->comment_list[i];
        size_t j;
        for (j = 0; j < kl && e[j] != '\0'; j++) {
            char a = e[j];
            if (a >= 'a' && a <= 'z') {
                a = (char)(a - 'a' + 'A');
            }
            if (a != key[j]) {
                break;
            }
        }
        if (j == kl && e[kl] == '=') {
            strncpy(dst, e + kl + 1u, META_LEN - 1u);
            dst[META_LEN - 1u] = '\0';
            return;
        }
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    vorbis_state* s;
    stb_vorbis_info vi;
    stb_vorbis_comment vc;
    int err = 0;
    stb_vorbis* v = stb_vorbis_open_memory(src->data, (int)src->size, &err, NULL);
    if (v == NULL) {
        return NULL;
    }
    s = (vorbis_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        stb_vorbis_close(v);
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->v = v;
    s->size = src->size;
    vi = stb_vorbis_get_info(v);
    s->hz = vi.sample_rate;
    s->channels = (uint32_t)vi.channels;
    s->total_frames = stb_vorbis_stream_length_in_samples(v);
    vc = stb_vorbis_get_comment(v);
    comment(&vc, "TITLE", s->title);
    comment(&vc, "ARTIST", s->author);
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    vorbis_state* s = (vorbis_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        stb_vorbis_close(s->v);
        host->mem_free(host, s);
    }
}

/* Source frames: stb_vorbis mixes any channel count to the two asked for. */
static uint32_t pull(void* ctx, int16_t* out, uint32_t frames) {
    vorbis_state* s = (vorbis_state*)ctx;
    return (uint32_t)stb_vorbis_get_samples_short_interleaved(s->v, 2, out, (int)(frames * 2u));
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    vorbis_state* s = (vorbis_state*)state;
    (void)subsong;
    (void)voices;
    if (rate == 0u) {
        return 0;
    }
    s->rate = rate;
    stb_vorbis_seek_start(s->v);
    sel_stream_pcm_reset(&s->pcm, pull, s, s->hz, rate);
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    vorbis_state* s = (vorbis_state*)state;
    return sel_stream_pcm_read(&s->pcm, out, frames);
}

static int rp_seek(void* state, uint32_t ms) {
    vorbis_state* s = (vorbis_state*)state;
    if (!stb_vorbis_seek(s->v, (unsigned)((uint64_t)ms * s->hz / 1000u))) {
        return 0;
    }
    sel_stream_pcm_reset(&s->pcm, pull, s, s->hz, s->rate);
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    vorbis_state* s = (vorbis_state*)state;
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
    s->meta[n++].value = (uintptr_t) "Ogg Vorbis";
    s->meta[n].tag = SEL_META_CHANNELS;
    s->meta[n++].value = (uintptr_t)s->channels;
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)((uint64_t)s->total_frames * 1000u / s->hz);
    s->meta[n].tag = SEL_META_SIZE;
    s->meta[n++].value = (uintptr_t)s->size;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "stb_vorbis");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "ogg");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_MIN_CPU, 68020);
    set(&g_hooks[12], SEL_RP_NEEDS_FLOAT, 1);
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "stb_vorbis");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
