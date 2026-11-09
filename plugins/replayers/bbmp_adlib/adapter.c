/* bbmp Ad Lib helper formats (row I710as): a RENDER-kind adapter over bbmp's csrc/adlib engines (third_party/bbmp_adlib, GPL-3.0-or-later, clean room) that play
 * Ad Lib songs AdPlug does not: Pixel Painters (.fmf, .ftf), Sound Images Generation 2 (.sig), Lemmings' driver (.lem), First Samurai (.mus), Shadowlands (.cal),
 * Vibrants (.vib), Sagitta FM Tracker (.fmt), EdLib Packed (.edl, unpacked with zlib's blast and converted to a D00 song AdPlug plays). The engines drive an OPL chip; shim.cpp runs them on AdPlug's Nuked OPL3 (the adplug_engine target) and keeps the PCM. The extension picks
 * the engine, and the probe asks the engine to read the song's header (a dry run without sound), so a file laid out like another format is not claimed.
 * FM Tracker here is Sagitta's (id "FMTRK" + 1A ...); Davey Taylor's FM Tracker ("FMTracker\x01\x01") fails the engine's header read and stays with libopenmpt.
 * A damaged .edl (87 of 179 on Modland, by bbmp's own count) fails the unpack or the JCH header check and is not claimed. Not here: EdLib D01 (.d01, row I710aw). One song per file; subsongs are the engine's tracks; rendered whole at RP_START. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct adsong;
int adsong_probe(const uint8_t* data, size_t size, const char* name);
struct adsong* adsong_open(const uint8_t* data, size_t size, const char* name, uint32_t track, uint32_t rate);
uint32_t adsong_tracks(const struct adsong* s);
uint32_t adsong_duration_ms(const struct adsong* s);
const char* adsong_title(const struct adsong* s);
const char* adsong_author(const struct adsong* s);
const char* adsong_type(const struct adsong* s);
int adsong_render(struct adsong* s, int16_t** pcm, uint64_t* frames);
void adsong_close(struct adsong* s);

#define BBADLIB_LARGEST (1024u * 1024u)

typedef struct bbadlib_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    char name[256];
    struct adsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    uint32_t subsongs;
    sel_tag meta[5];
} bbadlib_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (data == NULL || name == NULL || size < 16u || size > BBADLIB_LARGEST) {
        return SEL_PROBE_NO;
    }
    return adsong_probe(data, size, name) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbadlib_state* s = (bbadlib_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        if (s->song != NULL) {
            adsong_close(s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbadlib_state* s;
    if (src->stream != NULL || src->data == NULL || src->path == NULL || src->size < 16u || src->size > BBADLIB_LARGEST) {
        return NULL;
    }
    s = (bbadlib_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    strncpy(s->name, src->path, sizeof s->name - 1u);
    s->song = adsong_open(src->data, src->size, s->name, 0u, s->rate);
    if (s->song == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    s->subsongs = adsong_tracks(s->song);
    info->subsong_count = s->subsongs;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbadlib_state* s = (bbadlib_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= s->subsongs) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (s->song != NULL) {
        adsong_close(s->song);
    }
    s->song = adsong_open(s->data, s->size, s->name, subsong, rate);
    if (s->song == NULL) {
        return 0;
    }
    return adsong_render(s->song, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbadlib_state* s = (bbadlib_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbadlib_state* s = (bbadlib_state*)state;
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
    bbadlib_state* s = (bbadlib_state*)state;
    uint32_t n = 0;
    const char* title = adsong_title(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (adsong_author(s->song)[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)adsong_author(s->song);
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)adsong_type(s->song);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)adsong_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_adlib");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "fmf,ftf,sig,lem,mus,cal,vib,fmt,edl");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_adlib");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
