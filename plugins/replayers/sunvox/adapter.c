/* SunVox replayer (row I706a): a RENDER-kind adapter over the BSD SunVox engine (third_party/sunvox_bsd, Alex Zolotov
 * 2002-2009) for .sunvox songs. The engine is old: songs saved by newer SunVox versions (VERS above what the 2009 engine
 * writes) are refused in probe and open, never handed to the engine. The song plays once to its end (the engine's own
 * end-of-song flag); safety stop 20 minutes. Samples live inside the file, so there are no companion files. The engine
 * keeps global state: one song at a time (a second open fails). */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>


struct sunvox_glue;
int sunvox_glue_check(const unsigned char* d, unsigned long n);
struct sunvox_glue* sunvox_glue_open(const unsigned char* data, unsigned long size, int rate);
unsigned sunvox_glue_render(struct sunvox_glue* g, short* out, unsigned frames);
const char* sunvox_glue_title(struct sunvox_glue* g);
void sunvox_glue_close(struct sunvox_glue* g);

#define SV_MAX_SECONDS 1200u

typedef struct sv_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    struct sunvox_glue* eng;
    uint32_t rate;
    uint64_t frames;
    int ended;
    char title[40];
    sel_tag meta[3];
} sv_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return sunvox_glue_check(data, size) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    sv_state* s = (sv_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        sunvox_glue_close(s->eng);
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    sv_state* s;
    if (src->stream != NULL || src->data == NULL || src->size > 0x7FFFFFFFu || sunvox_glue_check(src->data, src->size) != 0) {
        return NULL;
    }
    s = (sv_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
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
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int restart(sv_state* s) {
    const char* t;
    size_t i, n = 0;
    sunvox_glue_close(s->eng);
    s->eng = sunvox_glue_open(s->data, (unsigned long)s->size, (int)s->rate);
    s->frames = 0;
    s->ended = 0;
    s->title[0] = '\0';
    if (s->eng == NULL) {
        return 0;
    }
    t = sunvox_glue_title(s->eng);
    for (i = 0; t[i] != '\0' && n < sizeof s->title - 1u; i++) {
        if ((unsigned char)t[i] >= 0x20u && (unsigned char)t[i] <= 0x7Eu) {
            s->title[n++] = t[i];
        }
    }
    s->title[n] = '\0';
    return 1;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    sv_state* s = (sv_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->rate = rate;
    return restart(s);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    sv_state* s = (sv_state*)state;
    uint32_t n;
    if (s->eng == NULL || s->ended) {
        return 0;
    }
    n = sunvox_glue_render(s->eng, (short*)out, frames);
    s->frames += n;
    if (n < frames || s->frames >= (uint64_t)s->rate * SV_MAX_SECONDS) {
        s->ended = 1;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    sv_state* s = (sv_state*)state;
    int16_t scratch[2048 * 2];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    if (s->eng == NULL || !restart(s)) {
        return 0;
    }
    /* No position call in the engine glue: restart and mix forward, discarding the output. */
    while (left > 0u && !s->ended) {
        uint32_t want = left > 2048u ? 2048u : (uint32_t)left;
        uint32_t got = rp_render(s, scratch, want);
        if (got == 0u) {
            break;
        }
        left -= got;
    }
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    sv_state* s = (sv_state*)state;
    size_t k = 0;
    s->meta[k].tag = SEL_META_FORMAT;
    s->meta[k++].value = (uintptr_t) "SunVox";
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sunvox");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sunvox");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sunvox");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
