/* bbmp ay replayer (row I709a): a RENDER-kind adapter over the bbmp csrc/ay engines (third_party/bbmp_ay, GPL-3.0-or-later)
 * on the Ayumi chip emulator (third_party/ayumi, MIT) for three ZX Spectrum formats: Flash Tracker (.fls, no magic),
 * Fuxoft AY Language (.fxm, no magic; .amad = AY file "ZXAYAMAD") and Sound Tracker compiled by Raxoft (AY file "ZXAYSTRC").
 * "ay" is claimed too, but the probe answers YES only for those two AY types, so ZXAYEMUL files stay with gme. The engines
 * need the rate at open time, so RP_OPEN opens once at 44100 Hz to learn the subsong count and metadata and RP_START
 * opens again at the real rate. A song plays until its engine reports the end; the safety stop is 20 minutes. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct bbay;
struct bbay* bbay_open(int kind, const uint8_t* data, uint32_t size, uint32_t track, uint32_t rate, uint32_t* tracks,
                       uint32_t* duration_ms, char* title, char* author, char* comment, uint32_t text_max);
uint32_t bbay_render(struct bbay* s, int16_t* out, uint32_t frames);
void bbay_close(struct bbay* s);

#define BBAY_MAX_SECONDS 1200u
#define BBAY_TEXT 256u

enum { KIND_FLS = 0, KIND_FXM = 1, KIND_STRC = 2, KIND_NONE = -1 };

typedef struct bbay_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int kind;
    struct bbay* engine;
    uint32_t rate;
    uint32_t subsong;
    uint32_t tracks;
    uint32_t duration_ms;
    uint64_t frames;
    int ended;
    char title[BBAY_TEXT];
    char author[BBAY_TEXT];
    char comment[BBAY_TEXT];
    sel_tag meta[7];
} bbay_state;

static int ext_is(const char* name, const char* ext) {
    const char* dot;
    size_t i;
    if (name == NULL || (dot = strrchr(name, '.')) == NULL || strlen(dot + 1) != strlen(ext)) {
        return 0;
    }
    for (i = 0; ext[i] != '\0'; i++) {
        if ((dot[1 + i] | 0x20) != ext[i]) {
            return 0;
        }
    }
    return 1;
}

/* The one decider of which engine owns a file: the AY magic first, then the extension of the formats with no magic. */
static int kind_of(const uint8_t* data, size_t size, const char* name) {
    if (size >= 8u && memcmp(data, "ZXAYAMAD", 8) == 0) {
        return KIND_FXM;
    }
    if (size >= 8u && memcmp(data, "ZXAYSTRC", 8) == 0) {
        return KIND_STRC;
    }
    if (ext_is(name, "fls")) {
        return KIND_FLS;
    }
    if (ext_is(name, "fxm")) {
        return KIND_FXM;
    }
    return KIND_NONE;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (size < 64u) {
        return SEL_PROBE_NO;
    }
    return kind_of(data, size, name) != KIND_NONE ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbay_state* s = (bbay_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        bbay_close(s->engine);
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

static struct bbay* open_engine(bbay_state* s, uint32_t track, uint32_t rate) {
    return bbay_open(s->kind, s->data, (uint32_t)s->size, track, rate, &s->tracks, &s->duration_ms, s->title, s->author,
                     s->comment, BBAY_TEXT);
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbay_state* s;
    struct bbay* probe;
    if (src->stream != NULL || src->data == NULL || src->size < 64u || src->size > 0x100000u) {
        return NULL;
    }
    s = (bbay_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->kind = kind_of(src->data, src->size, src->path);
    s->data = (uint8_t*)host->mem_alloc(host, src->size, SEL_MEM_FAST);
    if (s->kind == KIND_NONE || s->data == NULL) {
        host->mem_free(host, s->data);
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    s->size = src->size;
    probe = open_engine(s, 0u, 44100u);
    if (probe == NULL) { /* the engine refuses a file that is not really its format */
        rp_close(s);
        return NULL;
    }
    bbay_close(probe);
    info->voice_count = 0;
    info->subsong_count = s->tracks;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int restart(bbay_state* s, uint32_t subsong) {
    bbay_close(s->engine);
    s->engine = open_engine(s, subsong, s->rate);
    s->frames = 0;
    s->ended = 0;
    return s->engine != NULL;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbay_state* s = (bbay_state*)state;
    (void)voices;
    if (rate == 0u || subsong >= s->tracks) {
        return 0;
    }
    s->rate = rate;
    s->subsong = subsong;
    return restart(s, subsong);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbay_state* s = (bbay_state*)state;
    uint32_t n;
    if (s->engine == NULL || s->ended) {
        return 0;
    }
    n = bbay_render(s->engine, out, frames);
    s->frames += n;
    if (n < frames || s->frames >= (uint64_t)s->rate * BBAY_MAX_SECONDS) {
        s->ended = 1;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbay_state* s = (bbay_state*)state;
    int16_t scratch[2048 * 2];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    /* No random access in the engines: restart the subsong and run it forward, discarding the output. */
    if (s->engine == NULL || !restart(s, s->subsong)) {
        return 0;
    }
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
    bbay_state* s = (bbay_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->kind == KIND_FLS ? "Flash Tracker" : s->kind == KIND_FXM ? "Fuxoft AY Language" : "Sound Tracker (Raxoft)");
    if (s->duration_ms != 0u) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)s->duration_ms;
    }
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_ay");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "fls,fxm,amad,strc,ay");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_RP_PRIORITY, 10); /* above gme (0), which also claims .ay; the probe answers YES only for the two AY types and the two extensions */
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_ay");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
