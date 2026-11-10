/* Sawteeth replayer (row I1242e): a RENDER-kind adapter over Jonas Norberg's Sawteeth player library (third_party/sawteeth, MIT), Sawteeth songs
 * (.st, binary "SWTD" or text "SWTT"). The extension .st is shared with Ultimate Soundtracker and other trackers, so the probe claims only a file that
 * starts with one of the two magics. No companion files. One subsong; the first cycle of the song (up to the loop point) is rendered whole at RP_START
 * at the library's fixed 44100 Hz and resampled linearly for any other rate. The library is float math: SEL_RP_NEEDS_FLOAT hides it on an Amiga without an FPU. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct swtsong;
struct swtsong* swtsong_open(const uint8_t* data, uint32_t size);
const char* swtsong_title(struct swtsong* o);
const char* swtsong_author(struct swtsong* o);
int swtsong_render(struct swtsong* o, int16_t** pcm, uint64_t* frames);
void swtsong_close(struct swtsong* o);

#define SWT_NATIVE_RATE 44100u

typedef struct swt_state {
    const sel_host* host;
    struct swtsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} swt_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size < 6u) {
        return SEL_PROBE_NO;
    }
    if (memcmp(data, "SWTD", 4) == 0 || memcmp(data, "SWTT", 4) == 0) {
        return SEL_PROBE_YES;
    }
    return SEL_PROBE_NO;
}

static void rp_close(void* state) {
    swt_state* s = (swt_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        if (s->song != NULL) {
            swtsong_close(s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    swt_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < 6u) {
        return NULL;
    }
    s = (swt_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->rate = SWT_NATIVE_RATE;
    s->song = swtsong_open(src->data, (uint32_t)src->size);
    if (s->song == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    swt_state* s = (swt_state*)state;
    int16_t* native = NULL;
    uint64_t native_frames = 0;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (!swtsong_render(s->song, &native, &native_frames) || native == NULL) {
        return 0;
    }
    if (rate == SWT_NATIVE_RATE) {
        s->pcm = native;
        s->frames = native_frames;
        return 1;
    }
    {
        uint64_t n = native_frames * rate / SWT_NATIVE_RATE;
        uint64_t i;
        int16_t* out = (int16_t*)malloc((size_t)n * 4u + 4u);
        if (out == NULL) {
            free(native);
            return 0;
        }
        for (i = 0; i < n; i++) {
            uint64_t num = i * SWT_NATIVE_RATE;
            uint64_t a = num / rate;
            uint32_t frac = (uint32_t)(((num % rate) * 65536u) / rate);
            uint64_t b = a + 1u < native_frames ? a + 1u : a;
            int ch;
            for (ch = 0; ch < 2; ch++) {
                int32_t x = native[a * 2u + (uint64_t)ch];
                int32_t y = native[b * 2u + (uint64_t)ch];
                out[i * 2u + (uint64_t)ch] = (int16_t)(x + (int32_t)(((int64_t)(y - x) * frac) >> 16));
            }
        }
        free(native);
        s->pcm = out;
        s->frames = n;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    swt_state* s = (swt_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    swt_state* s = (swt_state*)state;
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
    swt_state* s = (swt_state*)state;
    uint32_t n = 0;
    const char* title = swtsong_title(s->song);
    const char* author = swtsong_author(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "Sawteeth";
    if (s->pcm != NULL) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)(s->frames * 1000u / s->rate);
    }
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sawteeth");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "st");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sawteeth");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
