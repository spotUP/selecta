/* bbmp TSS replayer (row I710af): a RENDER-kind adapter over bbmp's csrc/retro tss_engine (third_party/bbmp_tss, BSD-3-Clause, a port of
 * Takashi Toyoshima's T'SoundSystem). Plays .tss (MML text, compiled on open) and .tsd (compiled data, magic "T'SoundSystem"). The song is
 * rendered at RP_START into one buffer that rp_render copies out. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int bbtss_render(const uint8_t* data, size_t size, uint32_t rate, int check_only, int16_t** pcm, uint64_t* frames, long* duration_ms, char* err,
                 size_t errn, char* title, size_t titlen);

typedef struct bbts_state {
    const sel_host* host;
    uint8_t* data;
    size_t size;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    int is_tsd;
    char title[128];
    sel_tag meta[4];
} bbts_state;

static int ext_is(const char* name, const char* a, const char* b) {
    size_t nl = name != NULL ? strlen(name) : 0u;
    size_t i;
    if (nl < 5u || name[nl - 4u] != '.') {
        return 0;
    }
    for (i = 0; i < 3u; i++) {
        char c = (char)(name[nl - 3u + i] | 0x20);
        if (c != a[i]) {
            break;
        }
    }
    if (i == 3u) {
        return 1;
    }
    for (i = 0; i < 3u; i++) {
        if ((char)(name[nl - 3u + i] | 0x20) != b[i]) {
            return 0;
        }
    }
    return 1;
}

static int is_tsd_data(const uint8_t* d, size_t n) {
    return n > 14u && memcmp(d, "T'SoundSystem", 14) == 0;
}

/* MML text: a #TITLE directive in the first 4 KB. */
static int is_mml(const uint8_t* d, size_t n) {
    size_t i;
    size_t lim = n < 4096u ? n : 4096u;
    for (i = 0; i + 6u < lim; i++) {
        if (d[i] == '#' && (d[i + 1] | 0x20) == 't' && (d[i + 2] | 0x20) == 'i' && (d[i + 3] | 0x20) == 't' && (d[i + 4] | 0x20) == 'l' &&
            (d[i + 5] | 0x20) == 'e') {
            return 1;
        }
    }
    return 0;
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (!ext_is(name, "tss", "tsd")) {
        return SEL_PROBE_NO;
    }
    return is_tsd_data(data, size) || is_mml(data, size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbts_state* s = (bbts_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        free(s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbts_state* s;
    char err[256];
    char title[128];
    title[0] = '\0';
    if (src->stream != NULL || src->data == NULL || (!is_tsd_data(src->data, src->size) && !is_mml(src->data, src->size)) ||
        !bbtss_render(src->data, src->size, 44100u, 1, NULL, NULL, NULL, err, sizeof err, title, sizeof title)) {
        return NULL;
    }
    s = (bbts_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    strcpy(s->title, title);
    s->is_tsd = is_tsd_data(src->data, src->size);
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
    bbts_state* s = (bbts_state*)state;
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
    if (!bbtss_render(s->data, s->size, rate, 0, &s->pcm, &s->frames, &dur, err, sizeof err, NULL, 0) || s->frames == 0u) {
        free(s->pcm);
        s->pcm = NULL;
        s->frames = 0;
        return 0;
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbts_state* s = (bbts_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbts_state* s = (bbts_state*)state;
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
    bbts_state* s = (bbts_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->is_tsd ? "T'SoundSystem (TSD)" : "T'SoundSystem (MML)");
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_tss");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "tss,tsd");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_tss");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
