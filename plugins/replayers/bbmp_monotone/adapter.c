/* bbmp Monotone replayer (row I710ao): a RENDER-kind adapter over bbmp's csrc/pcmusic/monotone.cpp (third_party/bbmp_monotone, GPL-3.0-or-later port of the
 * public-domain MONOTONE tracker player), Jim Leonard's PC speaker tracker songs (.mon). The extension .mon is shared with nuade (maniacsofnoise), so the probe checks
 * MONOTONE's own header (Pascal string "\x08MONOTONE", version 1, 1..12 tracks, cell size 2, non-empty order list) and never claims a file without it. No
 * companion files. One subsong; the song is rendered whole at RP_START. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct monsong;
struct monsong* monsong_open(const uint8_t* data, uint32_t size);
uint32_t monsong_duration_ms(const struct monsong* o);
const char* monsong_title(const struct monsong* o);
const char* monsong_type(const struct monsong* o);
const char* monsong_comment(const struct monsong* o);
int monsong_render(struct monsong* o, uint32_t rate, int16_t** pcm, uint64_t* frames);
void monsong_close(struct monsong* o);

#define BBMON_SMALLEST 351u
#define BBMON_LARGEST (16u << 20)

typedef struct bbmon_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    struct monsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} bbmon_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    unsigned tracks;
    (void)name;
    if (size < BBMON_SMALLEST || size > BBMON_LARGEST || memcmp(data, "\x08MONOTONE", 9) != 0) {
        return SEL_PROBE_NO;
    }
    tracks = data[93];
    if (data[91] != 1u || tracks < 1u || tracks > 12u || data[94] != 2u || data[95] == 0xFFu) {
        return SEL_PROBE_NO;
    }
    return SEL_PROBE_YES;
}

static void rp_close(void* state) {
    bbmon_state* s = (bbmon_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        if (s->song != NULL) {
            monsong_close(s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbmon_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < BBMON_SMALLEST) {
        return NULL;
    }
    s = (bbmon_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    s->song = monsong_open(src->data, (uint32_t)src->size);
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
    bbmon_state* s = (bbmon_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (s->song != NULL) {
        monsong_close(s->song);
    }
    s->song = monsong_open(s->data, (uint32_t)s->size);
    if (s->song == NULL) {
        return 0;
    }
    return monsong_render(s->song, rate, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbmon_state* s = (bbmon_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbmon_state* s = (bbmon_state*)state;
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
    bbmon_state* s = (bbmon_state*)state;
    uint32_t n = 0;
    const char* title = monsong_title(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (monsong_comment(s->song)[0] != '\0') {
        s->meta[n].tag = SEL_META_MESSAGE;
        s->meta[n++].value = (uintptr_t)monsong_comment(s->song);
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)monsong_type(s->song);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)monsong_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_monotone");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "mon");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_monotone");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
