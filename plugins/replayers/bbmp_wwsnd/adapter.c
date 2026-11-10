/* bbmp Westwood SND replayer (row I710aj): a RENDER-kind adapter over bbmp's csrc/retro/wwsnd_engine.cpp (third_party/bbmp_wwsnd, GPL-3.0-or-later),
 * Westwood PC speaker files (.snd: Eye of the Beholder 1 and 2, Legend of Kyrandia): one square wave voice of byte code. The probe wants a track table
 * of 120 bytes followed by an offset table whose first entry (its own size) is 300, 500, 600 or 1000, and the engine must accept the file (PC speaker byte code; the AdLib .adl files share the tables).
 * No companion files. Each program the track table names is a subsong; the track is rendered whole at RP_START. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct wwsong;
struct wwsong* wwsong_open(const uint8_t* data, uint32_t size, uint32_t track, uint32_t rate);
uint32_t wwsong_duration_ms(const struct wwsong* o);
const char* wwsong_title(const struct wwsong* o);
const char* wwsong_type(const struct wwsong* o);
const char* wwsong_author(const struct wwsong* o);
uint32_t wwsong_tracks(const struct wwsong* o);
int wwsong_render(struct wwsong* o, int16_t** pcm, uint64_t* frames);
void wwsong_close(struct wwsong* o);

#define BBWW_SMALLEST 124u

typedef struct bbww_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    struct wwsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    uint32_t subsongs;
    sel_tag meta[5];
} bbww_state;


static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    unsigned table;
    struct wwsong* song;
    (void)name;
    if (size < BBWW_SMALLEST) {
        return SEL_PROBE_NO;
    }
    table = (unsigned)data[120] | ((unsigned)data[121] << 8);
    if (table != 300u && table != 500u && table != 600u && table != 1000u) {
        return SEL_PROBE_NO;
    }
    /* The AdLib .adl files of the same games have the same track table and offset table (1000 included), so the tables alone do not tell the
     * two formats apart: only the programs do (PC speaker byte code to its end, versus AdLib data). The engine checks that at open, so the
     * probe asks it: a file it refuses is not ours and goes to the next replayer (adplug). */
    song = wwsong_open(data, (uint32_t)size, 0u, 44100u);
    if (song == NULL) {
        return SEL_PROBE_NO;
    }
    wwsong_close(song);
    return SEL_PROBE_YES;
}

static void rp_close(void* state) {
    bbww_state* s = (bbww_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        if (s->song != NULL) {
            wwsong_close(s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbww_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < BBWW_SMALLEST) {
        return NULL;
    }
    s = (bbww_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    s->song = wwsong_open(src->data, (uint32_t)src->size, 0u, s->rate);
    if (s->song == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    s->subsongs = wwsong_tracks(s->song);
    info->subsong_count = s->subsongs;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbww_state* s = (bbww_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= s->subsongs) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (s->song != NULL) {
        wwsong_close(s->song);
    }
    s->song = wwsong_open(s->data, (uint32_t)s->size, subsong, rate);
    if (s->song == NULL) {
        return 0;
    }
    return wwsong_render(s->song, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbww_state* s = (bbww_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbww_state* s = (bbww_state*)state;
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
    bbww_state* s = (bbww_state*)state;
    uint32_t n = 0;
    const char* title = wwsong_title(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (wwsong_author(s->song)[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)wwsong_author(s->song);
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)wwsong_type(s->song);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)wwsong_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_wwsnd");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "snd");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_wwsnd");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
