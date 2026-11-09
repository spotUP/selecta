/* bbmp Synder replayer (row I710an): a RENDER-kind adapter over bbmp's csrc/synder/bbmp-synder.cpp (third_party/bbmp_synder, GPL-3.0-or-later), Synder
 * SNG-Player and SNG-Player Stereo songs (.sng, C64 tunes recorded as synthesizer settings). The extension .sng is shared with AdPlug, nuade, GoatTracker
 * ("GTS!", "GTS2".."GTS5") and SCC-Musixx (bbmp_smx), so the probe checks the song's own header (bytes "SYN", type byte, voice count, frame rate, text length,
 * frame data size a whole number of frames) and never claims a file without it. Synder Tracker songs ("SYNDAPS0", "SYNDERS0") are refused. No companion
 * files. One subsong; the song is rendered whole at RP_START. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct synsong;
struct synsong* synsong_open(const uint8_t* data, uint32_t size, uint32_t rate);
uint32_t synsong_duration_ms(const struct synsong* o);
const char* synsong_title(const struct synsong* o);
const char* synsong_type(const struct synsong* o);
const char* synsong_author(const struct synsong* o);
int synsong_render(struct synsong* o, int16_t** pcm, uint64_t* frames);
void synsong_close(struct synsong* o);

#define BBSYN_SMALLEST 17u
#define BBSYN_LARGEST (16u << 20)

typedef struct bbsyn_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    struct synsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} bbsyn_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    unsigned type, voices, rate;
    unsigned long text_len, unpacked;
    (void)name;
    if (size < BBSYN_SMALLEST || size > BBSYN_LARGEST || memcmp(data, "SYN", 3) != 0) {
        return SEL_PROBE_NO;
    }
    if (size >= 8u && (memcmp(data, "SYNDAPS0", 8) == 0 || memcmp(data, "SYNDERS0", 8) == 0)) {
        return SEL_PROBE_NO;
    }
    type = data[3];
    voices = type & 15u;
    if ((type & 0xa0u) != 0u || voices < 1u || voices > 8u) {
        return SEL_PROBE_NO;
    }
    rate = data[4] | (unsigned)data[5] << 8;
    if (rate < 1u || rate > 1000u) {
        return SEL_PROBE_NO;
    }
    text_len = data[10] | (unsigned long)data[11] << 8;
    unpacked = data[12] | (unsigned long)data[13] << 8 | (unsigned long)data[14] << 16 | (unsigned long)data[15] << 24;
    if (16u + text_len > size || unpacked == 0u || unpacked % (6u * voices) != 0u) {
        return SEL_PROBE_NO;
    }
    return SEL_PROBE_YES;
}

static void rp_close(void* state) {
    bbsyn_state* s = (bbsyn_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        if (s->song != NULL) {
            synsong_close(s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbsyn_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < BBSYN_SMALLEST) {
        return NULL;
    }
    s = (bbsyn_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    s->song = synsong_open(src->data, (uint32_t)src->size, s->rate);
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
    bbsyn_state* s = (bbsyn_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (s->song != NULL) {
        synsong_close(s->song);
    }
    s->song = synsong_open(s->data, (uint32_t)s->size, rate);
    if (s->song == NULL) {
        return 0;
    }
    return synsong_render(s->song, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbsyn_state* s = (bbsyn_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbsyn_state* s = (bbsyn_state*)state;
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
    bbsyn_state* s = (bbsyn_state*)state;
    uint32_t n = 0;
    const char* title = synsong_title(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (synsong_author(s->song)[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)synsong_author(s->song);
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)synsong_type(s->song);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)synsong_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_synder");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sng");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_synder");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
