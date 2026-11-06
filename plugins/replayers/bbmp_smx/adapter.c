/* bbmp SCC-Musixx replayer (row I710am): a RENDER-kind adapter over bbmp's csrc/retro/smx_engine.cpp (third_party/bbmp_smx, GPL-3.0-or-later, on emu2212 MIT),
 * SCC-Musixx songs (.sng, MSX, Konami SCC chip). The extension .sng is shared with AdPlug (SNGPlay, Adlib Tracker, Faust), nuade (Richard Joseph, Zoundmonitor) and
 * GoatTracker, so the probe checks the song's own structure (the engine's checks: size, song length byte, position list, instrument names as text, pattern cells)
 * and never claims a file that is not laid out like an SCC-Musixx song. No companion files. One subsong; the song is rendered whole at RP_START. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct smxsong;
struct smxsong* smxsong_open(const uint8_t* data, uint32_t size, uint32_t track, uint32_t rate);
uint32_t smxsong_duration_ms(const struct smxsong* o);
const char* smxsong_title(const struct smxsong* o);
const char* smxsong_type(const struct smxsong* o);
const char* smxsong_author(const struct smxsong* o);
uint32_t smxsong_tracks(const struct smxsong* o);
int smxsong_render(struct smxsong* o, int16_t** pcm, uint64_t* frames);
void smxsong_close(struct smxsong* o);

#define BBSMX_SMALLEST 3557u
#define BBSMX_LARGEST 32768u

typedef struct bbsmx_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    struct smxsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    uint32_t subsongs;
    sel_tag meta[5];
} bbsmx_state;


static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    unsigned length, i, k, j, patterns, p, r, v;
    unsigned long cells = 0, bad = 0;
    static const unsigned voice_at[5] = {0, 5, 10, 15, 20};
    (void)name;
    if (size < BBSMX_SMALLEST || size > BBSMX_LARGEST) {
        return SEL_PROBE_NO;
    }
    length = data[0x780];
    if (length < 1u || length > 100u) {
        return SEL_PROBE_NO;
    }
    for (i = 0; i < length; i++) {
        if (data[0x781 + i] >= 20u) {
            return SEL_PROBE_NO;
        }
    }
    for (k = 0; k < 48u; k++) {
        for (j = 32; j < 40u; j++) {
            unsigned c = data[k * 40u + j];
            if (c != 0u && (c < 32u || c > 126u)) {
                return SEL_PROBE_NO;
            }
        }
    }
    patterns = (unsigned)((size - 2021u) / 0x600u);
    for (p = 0; p < patterns; p++) {
        for (r = 0; r < 64u; r++) {
            const uint8_t* row = data + 0x7e5 + (size_t)p * 0x600u + r * 24u;
            for (v = 0; v < 5u; v++) {
                cells++;
                if (row[voice_at[v] + 1] > 15u) {
                    bad++;
                } else if (v < 4u && row[voice_at[v] + 2] > 48u) {
                    bad++;
                }
            }
        }
    }
    return bad * 50u > cells ? SEL_PROBE_NO : SEL_PROBE_YES;
}

static void rp_close(void* state) {
    bbsmx_state* s = (bbsmx_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        if (s->song != NULL) {
            smxsong_close(s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbsmx_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < BBSMX_SMALLEST) {
        return NULL;
    }
    s = (bbsmx_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    s->song = smxsong_open(src->data, (uint32_t)src->size, 0u, s->rate);
    if (s->song == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    s->subsongs = smxsong_tracks(s->song);
    info->subsong_count = s->subsongs;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbsmx_state* s = (bbsmx_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= s->subsongs) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (s->song != NULL) {
        smxsong_close(s->song);
    }
    s->song = smxsong_open(s->data, (uint32_t)s->size, subsong, rate);
    if (s->song == NULL) {
        return 0;
    }
    return smxsong_render(s->song, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbsmx_state* s = (bbsmx_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbsmx_state* s = (bbsmx_state*)state;
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
    bbsmx_state* s = (bbsmx_state*)state;
    uint32_t n = 0;
    const char* title = smxsong_title(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (smxsong_author(s->song)[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)smxsong_author(s->song);
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)smxsong_type(s->song);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)smxsong_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_smx");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_smx");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
