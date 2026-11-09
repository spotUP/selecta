/* bbmp Digitrekker replayer (row I710ap): a RENDER-kind adapter over bbmp's csrc/pcmusic/digitrekker.cpp (third_party/bbmp_digitrekker, GPL-3.0-or-later),
 * Horst Beham's DigiTrekker modules (.dtm). The extension .dtm is shared with Digital Tracker (libopenmpt, magic "D.T.") and DeFy (adplug, magic "DeFy"), so the
 * probe checks DigiTrekker's own structure: "SONG" + a chunk length equal to the file size minus 8, then a NAME chunk first and a complete walk of the top-level
 * chunks that finds INFO (1..64 channels) and PATT. No companion files. One subsong; the song is rendered whole at RP_START. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct dtmsong;
struct dtmsong* dtmsong_open(const uint8_t* data, uint32_t size);
uint32_t dtmsong_duration_ms(const struct dtmsong* o);
const char* dtmsong_title(const struct dtmsong* o);
const char* dtmsong_type(const struct dtmsong* o);
const char* dtmsong_comment(const struct dtmsong* o);
int dtmsong_render(struct dtmsong* o, uint32_t rate, int16_t** pcm, uint64_t* frames);
void dtmsong_close(struct dtmsong* o);

#define BBDTM_SMALLEST 64u
#define BBDTM_LARGEST (16u << 20)

typedef struct bbdtm_state {
    const sel_host* host;
    const uint8_t* data;
    size_t size;
    struct dtmsong* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} bbdtm_state;

static uint32_t dtm_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t at = 8;
    int info = 0, patt = 0;
    (void)name;
    if (size < BBDTM_SMALLEST || size > BBDTM_LARGEST || memcmp(data, "SONG", 4) != 0 || memcmp(data + 8, "NAME", 4) != 0) {
        return SEL_PROBE_NO;
    }
    if ((size_t)dtm_le32(data + 4) != size - 8u) {
        return SEL_PROBE_NO;
    }
    while (at + 8u <= size) {
        size_t len = dtm_le32(data + at + 4);
        if (len > size - at - 8u) {
            return SEL_PROBE_NO;
        }
        if (memcmp(data + at, "INFO", 4) == 0 && len >= 10u) {
            unsigned ch = (unsigned)data[at + 8] | ((unsigned)data[at + 9] << 8);
            if (ch < 1u || ch > 64u) {
                return SEL_PROBE_NO;
            }
            info = 1;
        } else if (memcmp(data + at, "PATT", 4) == 0) {
            patt = 1;
        }
        at += 8u + len;
    }
    return (info && patt && at == size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbdtm_state* s = (bbdtm_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        if (s->song != NULL) {
            dtmsong_close(s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbdtm_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < BBDTM_SMALLEST) {
        return NULL;
    }
    s = (bbdtm_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = 44100u;
    s->song = dtmsong_open(src->data, (uint32_t)src->size);
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
    bbdtm_state* s = (bbdtm_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    if (s->song != NULL) {
        dtmsong_close(s->song);
    }
    s->song = dtmsong_open(s->data, (uint32_t)s->size);
    if (s->song == NULL) {
        return 0;
    }
    return dtmsong_render(s->song, rate, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbdtm_state* s = (bbdtm_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbdtm_state* s = (bbdtm_state*)state;
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
    bbdtm_state* s = (bbdtm_state*)state;
    uint32_t n = 0;
    const char* title = dtmsong_title(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (dtmsong_comment(s->song)[0] != '\0') {
        s->meta[n].tag = SEL_META_MESSAGE;
        s->meta[n++].value = (uintptr_t)dtmsong_comment(s->song);
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)dtmsong_type(s->song);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)dtmsong_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_digitrekker");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "dtm");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_digitrekker");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
