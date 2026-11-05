/* Klystrack replayer (row I708a): a RENDER-kind adapter over the klystron sound engine (third_party/klystron, MIT) for
 * klystrack chiptune songs (.kt). The song loads from memory (the engine's own RWops, no SDL), the sequencer runs on the
 * engine's tick callback and the mix is pulled with cyd_output_buffer_stereo. The adapter plays the song once (length from
 * mus_get_playtime_at over the song's rows) plus a short tail, then ends; the song's loop point is not followed. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "music.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KT_SIG "cyd!song"
#define KT_SIG_LEN 8u
#define KT_MIN_SIZE 64u
#define KT_TAIL_SECONDS 2u
#define KT_MAX_SECONDS 1200u
#define KT_CHUNK 1024u

typedef struct kt_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    CydEngine cyd;
    MusEngine mus;
    MusSong song;
    CydWavetableEntry* wave; /* the song's wavetable lives in the engine; kept to free it */
    int live;
    uint32_t rate;
    uint64_t frames;
    uint64_t end_frames;
    int ended;
    int16_t mix[KT_CHUNK * 2u];
    char title[40];
    sel_tag meta[4];
} kt_state;


static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (size >= KT_SIG_LEN + 1u && memcmp(data, KT_SIG, KT_SIG_LEN) == 0) {
        return SEL_PROBE_YES;
    }
    (void)name;
    return SEL_PROBE_NO;
}

static int mem_read(struct RWops* ctx, void* ptr, int size, int maxnum) {
    uint32_t want = (uint32_t)size * (uint32_t)maxnum;
    uint32_t left = ctx->mem.length - ctx->mem.ptr;
    uint32_t n = want < left ? want : left;
    memcpy(ptr, (const uint8_t*)ctx->mem.base + ctx->mem.ptr, n);
    ctx->mem.ptr += n;
    return size > 0 ? (int)(n / (uint32_t)size) : 0;
}

static int mem_close(struct RWops* ctx) {
    (void)ctx;
    return 1;
}

static void engine_free(kt_state* s) {
    if (s->live) {
        mus_set_song(&s->mus, NULL, 0);
        cyd_set_callback(&s->cyd, NULL, NULL, 1);
        mus_free_song(&s->song);
        cyd_deinit(&s->cyd);
        s->live = 0;
    }
}

static void rp_close(void* state) {
    kt_state* s = (kt_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        engine_free(s);
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

/* Loads the song into a fresh engine at `rate` and starts the sequencer at row 0. */
static int engine_start(kt_state* s, uint32_t rate) {
    RWops rw;
    uint32_t ms;
    engine_free(s);
    memset(&s->cyd, 0, sizeof s->cyd);
    memset(&s->mus, 0, sizeof s->mus);
    memset(&s->song, 0, sizeof s->song);
    cyd_init(&s->cyd, (Uint16)rate, CYD_MAX_CHANNELS);
    s->cyd.flags |= CYD_SINGLE_THREAD;
    mus_init_engine(&s->mus, &s->cyd);
    memset(&rw, 0, sizeof rw);
    rw.read = mem_read;
    rw.close = mem_close;
    rw.mem.base = s->data;
    rw.mem.length = (uint32_t)s->size;
    s->live = 1;
    if (!mus_load_song_RW(&rw, &s->song, s->cyd.wavetable_entries)) {
        cyd_deinit(&s->cyd);
        s->live = 0;
        return 0;
    }
    cyd_set_callback(&s->cyd, mus_advance_tick, &s->mus, s->song.song_rate);
    mus_set_song(&s->mus, &s->song, 0);
    ms = mus_get_playtime_at(&s->song, s->song.song_length);
    s->end_frames = (uint64_t)ms * rate / 1000u + (uint64_t)rate * KT_TAIL_SECONDS;
    if (s->end_frames > (uint64_t)rate * KT_MAX_SECONDS) {
        s->end_frames = (uint64_t)rate * KT_MAX_SECONDS;
    }
    s->frames = 0;
    s->ended = 0;
    s->rate = rate;
    return 1;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    kt_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < KT_MIN_SIZE || src->size > 0x7fffffffu
        || memcmp(src->data, KT_SIG, KT_SIG_LEN) != 0) {
        return NULL;
    }
    s = (kt_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
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
    /* Load once at the common rate to validate the file (and read the title); rp_start reloads at the real rate. */
    if (!engine_start(s, 44100u)) {
        rp_close(s);
        return NULL;
    }
    strncpy(s->title, s->song.title, sizeof s->title - 1u);
    info->voice_count = (uint32_t)s->song.num_channels;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    kt_state* s = (kt_state*)state;
    (void)voices;
    if (rate == 0u || rate > 65535u || subsong != 0u) {
        return 0;
    }
    return engine_start(s, rate);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    kt_state* s = (kt_state*)state;
    uint64_t left;
    uint32_t n, done = 0;
    if (!s->live || s->ended) {
        return 0;
    }
    left = s->end_frames - s->frames;
    n = (uint32_t)(left < frames ? left : frames);
    while (done < n) {
        uint32_t c = n - done < KT_CHUNK ? n - done : KT_CHUNK, i;
        memset(s->mix, 0, c * 2u * sizeof(int16_t)); /* the engine adds into the buffer */
        cyd_output_buffer_stereo(&s->cyd, (Uint8*)s->mix, (int)(c * 2u * sizeof(int16_t)));
        for (i = 0; i < c * 2u; i++) {
            int32_t v = (int32_t)s->mix[i] * 2; /* the engine mixes at a quarter of full scale for channel headroom */
            out[done * 2u + i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
        }
        done += c;
    }
    s->frames += n;
    if (n < frames) {
        s->ended = 1;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    kt_state* s = (kt_state*)state;
    int16_t scratch[KT_CHUNK * 2u];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    if (!s->live || !engine_start(s, s->rate)) {
        return 0;
    }
    /* Channels carry state (envelopes, oscillator phase): run the song forward and discard the output. */
    while (left > 0u && !s->ended) {
        uint32_t want = left > KT_CHUNK ? KT_CHUNK : (uint32_t)left;
        uint32_t got = rp_render(s, scratch, want);
        if (got == 0u) {
            break;
        }
        left -= got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    kt_state* s = (kt_state*)state;
    size_t k = 0;
    s->meta[k].tag = SEL_META_FORMAT;
    s->meta[k++].value = (uintptr_t) "Klystrack";
    if (s->title[0] != '\0') {
        s->meta[k].tag = SEL_META_TITLE;
        s->meta[k++].value = (uintptr_t)s->title;
    }
    s->meta[k].tag = SEL_TAG_END;
    s->meta[k].value = 0;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "klystron");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "kt");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10);
    set(&g_hooks[12], SEL_RP_NEEDS_FLOAT, 1);
    set(&g_hooks[13], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "klystron");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
