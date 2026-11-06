/* bbmp maxtrax replayer (row I710k): a RENDER-kind adapter over bbmp's csrc/amiga/maxtrax_engine (third_party/bbmp_maxtrax;
 * ScummVM's MaxTrax player and Paula, GPL 3 or later), Joe Pearce's MaxTrax songs of Amiga games (.mxtx, header "MXTX"). Each
 * score with notes is a subsong, rendered whole at RP_START. A score file without samples reads them from another .mxtx of the
 * song's folder (the engine scans src->path's folder; no content companion can name it, so none is declared). */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

struct mxtxr;
int mxtxr_probe(const uint8_t* data, uint32_t size);
struct mxtxr* mxtxr_open(const uint8_t* data, uint32_t size, const char* path);
int mxtxr_tracks(const struct mxtxr* r);
const char* mxtxr_comment(const struct mxtxr* r);
uint32_t mxtxr_duration_ms(const struct mxtxr* r);
int mxtxr_render(struct mxtxr* r, int track, uint32_t rate, int16_t** pcm, uint64_t* frames);
void mxtxr_close(struct mxtxr* r);

typedef struct bbmx_state {
    const sel_host* host;
    struct mxtxr* song;
    int16_t* pcm;
    uint64_t frames;
    uint64_t pos;
    uint32_t rate;
    uint32_t ms;
    uint32_t subsong;
    sel_tag meta[5];
} bbmx_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    if (size < 10u || size > 0x4000000u) {
        return SEL_PROBE_NO;
    }
    return mxtxr_probe(data, (uint32_t)size) ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void rp_close(void* state) {
    bbmx_state* s = (bbmx_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        free(s->pcm);
        mxtxr_close(s->song);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    bbmx_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < 10u || src->size > 0x4000000u) {
        return NULL;
    }
    s = (bbmx_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->song = mxtxr_open(src->data, (uint32_t)src->size, src->path);
    if (s->song == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    info->voice_count = 0;
    info->subsong_count = (uint32_t)mxtxr_tracks(s->song);
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    bbmx_state* s = (bbmx_state*)state;
    (void)voices;
    if (rate < 8000u || rate > 192000u || subsong >= (uint32_t)mxtxr_tracks(s->song)) {
        return 0;
    }
    free(s->pcm);
    s->pcm = NULL;
    s->pos = 0;
    s->rate = rate;
    s->subsong = subsong;
    if (!mxtxr_render(s->song, (int)subsong, rate, &s->pcm, &s->frames)) {
        return 0;
    }
    s->ms = mxtxr_duration_ms(s->song);
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    bbmx_state* s = (bbmx_state*)state;
    uint64_t left = s->pcm != NULL ? s->frames - s->pos : 0u;
    uint32_t n = left < frames ? (uint32_t)left : frames;
    if (n > 0u) {
        memcpy(out, s->pcm + s->pos * 2u, (size_t)n * 4u);
        s->pos += n;
    }
    return n;
}

static int rp_seek(void* state, uint32_t ms) {
    bbmx_state* s = (bbmx_state*)state;
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
    bbmx_state* s = (bbmx_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "MaxTrax";
    if (s->pcm != NULL) {
        s->meta[n].tag = SEL_META_DURATION_MS;
        s->meta[n++].value = (uintptr_t)s->ms;
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
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "bbmp_maxtrax");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "mxtx");
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
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "bbmp_maxtrax");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
