/* bbmp fac replayer (row I710ak): a RENDER-kind adapter over bbmp's csrc/fac/fac_cr.cpp (third_party/bbmp_fac,
 * GPL-3.0-or-later), FAC SoundTracker for MSX (.mus; versions 1.0, 2.0, Pro; MSX-AUDIO Y8950 or MSX-MUSIC YM2413). The
 * format has no magic number, so the probe asks the engine's own loader (size 16391 +- header, settings page checks)
 * and answers YES only for a song it accepts; priority 10 puts it above adplug (0), which also claims .mus, but an
 * AdLib Visual Composer .mus fails the loader and stays with adplug. The drum kit <kit>.SM1/.SM2 is read from the song's
 * folder by the engine. The song is rendered whole at RP_START (the engine renders once, start to end plus tail). */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct facr;
int facr_probe(const uint8_t* data, uint32_t size, const char* path);
struct facr* facr_open(const uint8_t* data, uint32_t size, const char* path);
uint32_t facr_duration_ms(const struct facr* f);
const char* facr_title(const struct facr* f);
const char* facr_author(const struct facr* f);
const char* facr_type(const struct facr* f);
int facr_render(struct facr* f, uint32_t rate, int16_t** pcm, uint64_t* frames);
void facr_close(struct facr* f);

typedef struct bbfac_state {
    const sel_host* host;
    struct facr* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    sel_tag meta[5];
} bbfac_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    if (size < 16384u || size > 0x10000u) {
        return SEL_PROBE_NO;
    }
    return facr_probe(data, (uint32_t)size, name) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbfac_state* s = (bbfac_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        facr_close(s->song);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbfac_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < 16384u || src->size > 0x10000u) {
        return NULL;
    }
    s = (bbfac_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->song = facr_open(src->data, (uint32_t)src->size, src->path);
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
    bbfac_state* s = (bbfac_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong != 0u) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    return facr_render(s->song, rate, &s->pcm, &s->frames);
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbfac_state* s = (bbfac_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbfac_state* s = (bbfac_state*)state;
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
    bbfac_state* s = (bbfac_state*)state;
    uint32_t n = 0;
    const char* title = facr_title(s->song);
    const char* author = facr_author(s->song);
    if (title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)title;
    }
    if (author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)facr_type(s->song);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)facr_duration_ms(s->song);
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_fac");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "mus");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_fac");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
