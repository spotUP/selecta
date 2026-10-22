/* nuade adapter replayer (design doc section 4): bridges nuade's
 * NuadePlayer vtable to the Selecta replayer hooks. M0b links only the
 * protracker4 player (the selecta_nuade library). */
#include "shim.h"

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

#define NUADE_PLAYER_ID "protracker4"
#define META_LEN 128u

typedef struct nuade_state {
    const sel_host* host;
    const NuadePlayer* player;
    void* pdata;
    NuadeSongSource src;
    NuadeVoices voices;
    char title[META_LEN];
    char author[META_LEN];
    char format[META_LEN];
    sel_tag meta[6];
} nuade_state;

static const NuadePlayer* player(void) {
    return nuade_player_find(NUADE_PLAYER_ID);
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    const NuadePlayer* p = player();
    if (p == NULL || p->probe == NULL) {
        return SEL_PROBE_NO;
    }
    switch (p->probe(data, size, name)) {
    case NuadeProbe_Supported:
        return SEL_PROBE_YES;
    case NuadeProbe_Unsure:
        return SEL_PROBE_UNSURE;
    default:
        return SEL_PROBE_NO;
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    const NuadePlayer* p = player();
    nuade_state* s;
    NuadePlayerOpenResult r;
    if (p == NULL) {
        return NULL;
    }
    s = (nuade_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->player = p;
    s->src.data = src->data;
    s->src.size = src->size;
    s->src.path = src->path;
    s->src.io = sel_nuade_io_none();
    r = p->open(&s->src);
    if (r.user_data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    s->pdata = r.user_data;
    info->voice_count = r.info.voice_count;
    info->subsong_count = r.info.subsong_count;
    info->default_subsong = r.info.default_subsong;
    info->timebase_hz = r.info.timebase_hz;
    return s;
}

static void rp_close(void* state) {
    nuade_state* s = (nuade_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        s->player->close(s->pdata);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    nuade_state* s = (nuade_state*)state;
    s->voices.api = voices;
    return s->player->start(s->pdata, subsong, rate, &s->voices) ? 1 : 0;
}

static void rp_tick(void* state, sel_tick_result* out) {
    nuade_state* s = (nuade_state*)state;
    NuadeTickResult r = s->player->tick(s->pdata, &s->voices);
    out->length = r.length;
    out->flags = ((r.flags & NuadeTick_Looped) ? SEL_TICK_LOOPED : 0u) |
                 ((r.flags & NuadeTick_Ended) ? SEL_TICK_ENDED : 0u);
}

static void copy_meta(char* dst, const char* value) {
    strncpy(dst, value, META_LEN - 1u);
    dst[META_LEN - 1u] = '\0';
}

static void set_tag(void* ctx, const char* key, const char* value) {
    nuade_state* s = (nuade_state*)ctx;
    if (strcmp(key, NUADE_META_TITLE) == 0) {
        copy_meta(s->title, value);
    } else if (strcmp(key, NUADE_META_ARTIST) == 0) {
        copy_meta(s->author, value);
    } else if (strcmp(key, NUADE_META_FORMAT) == 0) {
        copy_meta(s->format, value);
    }
}

static void add_instrument(void* ctx, uint32_t index, const char* name) {
    (void)ctx;
    (void)index;
    (void)name;
}

static const sel_tag* rp_metadata(void* state) {
    nuade_state* s = (nuade_state*)state;
    NuadeMetadataSink sink;
    uint32_t n = 0;
    sink.ctx = s;
    sink.set_tag = set_tag;
    sink.add_instrument = add_instrument;
    if (s->player->metadata != NULL) {
        s->player->metadata(s->pdata, &sink);
    }
    s->meta[n].tag = SEL_META_TITLE;
    s->meta[n++].value = (uintptr_t)s->title;
    s->meta[n].tag = SEL_META_AUTHOR;
    s->meta[n++].value = (uintptr_t)s->author;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)s->format;
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

/* Filled at run time: a function pointer cast to an integer is not a
 * constant expression in C99. */
static sel_tag g_hooks[14];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    const NuadePlayer* p = player();
    if (host_api < 1u || p == NULL) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "nuade.protracker4");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_VOICE);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t)p->extensions);
    set(&g_hooks[4], SEL_RP_FORMATS, (uintptr_t)p->name);
    set(&g_hooks[5], SEL_RP_MAX_VOICES, SEL_NUADE_MAX_VOICES);
    set(&g_hooks[6], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[7], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[8], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[9], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[10], SEL_RP_TICK, SEL_TAG_FN(rp_tick));
    set(&g_hooks[11], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "nuade");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
