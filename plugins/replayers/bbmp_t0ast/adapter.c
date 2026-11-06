/* bbmp t0ast replayer (row I710ag): a RENDER-kind adapter over bbmp's csrc/t0ast engine (third_party/bbmp_t0ast, GPL-3.0-or-later), which plays
 * T0AST, "The 0ok Amazing Synth Tracker" songs (.t0ast, magic "T0AST"). The song is rendered at RP_START into one buffer that rp_render copies out;
 * the engine decides where the song ends (the tracker's WAV export rule). */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int bbt0ast_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms, char* err,
                   size_t errn, char* title, size_t titlen);

typedef struct bbt0_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[3];
} bbt0_state;

static int ext_is_t0ast(const char* name) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    return nl >= 7u && name[nl - 6u] == '.' && (name[nl - 5u] | 0x20) == 't' && name[nl - 4u] == '0' && (name[nl - 3u] | 0x20) == 'a' &&
           (name[nl - 2u] | 0x20) == 's' && (name[nl - 1u] | 0x20) == 't';
}

static int has_magic(const uint8_t* data, size_t size) {
    return size >= 16u && memcmp(data, "T0AST", 5) == 0;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    return ext_is_t0ast(name) && has_magic(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbt0_state* s = (bbt0_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbt0_state* s;
    char err[256];
    if (src->stream != NULL || src->data == NULL || !has_magic(src->data, src->size) ||
        !bbt0ast_render(src->data, src->size, 44100u, 1, NULL, NULL, NULL, err, sizeof err, NULL, 0u)) {
        return NULL;
    }
    s = (bbt0_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)malloc(src->size);
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

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbt0_state* s = (bbt0_state*)state;
    char err[256];
    long dur = 0;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->frames = s->pos = 0;
    s->rate = rate;
    if (!bbt0ast_render(s->data, s->size, rate, 0, &s->pcm, &s->frames, &dur, err, sizeof err, NULL, 0u) || s->frames == 0u) {
        free(s->pcm);
        s->pcm = NULL;
        s->frames = 0;
        return 0;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbt0_state* s = (bbt0_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbt0_state* s = (bbt0_state*)state;
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
    bbt0_state* s = (bbt0_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "T0AST (The 0ok Amazing Synth Tracker)";
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_t0ast");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "t0ast");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_t0ast");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
