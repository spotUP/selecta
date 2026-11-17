/* Arkos Tracker 3 replayer (I433b): a RENDER-kind replayer for .aks songs. at3_song.c imports the song model,
 * at3_player.c plays its PSG instruments on Ayumi. The duration and the seek run the sequencer without sound. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "at3_player.h"
#include "at3_song.h"
#include "probe.h"

#include <string.h>

typedef struct arkos_state {
    const sel_host* host;
    at3_song* song;
    size_t size;
    uint32_t subsong, rate, duration_ms;
    at3_player player;
    sel_tag meta[8];
} arkos_state;

static uint32_t ticks_to_ms(const at3_subsong* s, uint32_t ticks) {
    uint32_t mhz = s->replay_milli_hz > 0 ? (uint32_t)s->replay_milli_hz : 50000u;
    return (uint32_t)((uint64_t)ticks * 1000000u / mhz);
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    arkos_state* s;
    char err[96];
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (arkos_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->size = src->size;
    /* a subsong the player can't start (no PSG, no position) is refused here, not at start */
    if (at3_song_load(src->data, src->size, &s->song, err, sizeof err) != 0 ||
        at3_player_start(&s->player, s->song, 0, 44100) != 0) {
        at3_song_free(s->song);
        host->mem_free(host, s);
        return NULL;
    }
    s->duration_ms = ticks_to_ms(s->player.sub, at3_player_count_ticks(&s->player));
    info->voice_count = 0;
    info->subsong_count = (uint32_t)s->song->n_subsongs;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    arkos_state* s = (arkos_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        at3_song_free(s->song);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    arkos_state* s = (arkos_state*)state;
    (void)voices;
    if (rate == 0u || at3_player_start(&s->player, s->song, (int)subsong, (int)rate) != 0) {
        return 0;
    }
    s->subsong = subsong;
    s->rate = rate;
    s->duration_ms = ticks_to_ms(s->player.sub, at3_player_count_ticks(&s->player));
    return at3_player_start(&s->player, s->song, (int)subsong, (int)rate) == 0;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    return at3_player_render(&((arkos_state*)state)->player, out, frames);
}

static int rp_seek(void* state, uint32_t ms) {
    arkos_state* s = (arkos_state*)state;
    uint32_t mhz;
    if (s->rate == 0u || at3_player_start(&s->player, s->song, (int)s->subsong, (int)s->rate) != 0) {
        return 0;
    }
    mhz = s->player.sub->replay_milli_hz > 0 ? (uint32_t)s->player.sub->replay_milli_hz : 50000u;
    at3_player_skip(&s->player, (uint32_t)((uint64_t)ms * mhz / 1000000u));
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    arkos_state* s = (arkos_state*)state;
    uint32_t n = 0;
    if (s->song->title != NULL && s->song->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->song->title;
    }
    if (s->song->composer != NULL && s->song->composer[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->song->composer;
    } else if (s->song->author != NULL && s->song->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->song->author;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "Arkos Tracker 3";
    s->meta[n].tag = SEL_META_CHANNELS;
    s->meta[n++].value = (uintptr_t)s->player.n_channels;
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->duration_ms;
    s->meta[n].tag = SEL_META_SIZE;
    s->meta[n++].value = (uintptr_t)s->size;
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[14];
static sel_tag g_desc[5];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    size_t n;
    if (host_api < 1u) {
        return NULL;
    }
    n = arkos_static_hooks(g_hooks); /* NAME..NEEDS_FLOAT, shared with the Amiga manifest (probe.c) */
    arkos_set_tag(&g_hooks[n++], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    arkos_set_tag(&g_hooks[n++], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    arkos_set_tag(&g_hooks[n++], SEL_RP_START, SEL_TAG_FN(rp_start));
    arkos_set_tag(&g_hooks[n++], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    arkos_set_tag(&g_hooks[n++], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    arkos_set_tag(&g_hooks[n++], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    arkos_set_tag(&g_hooks[n], SEL_TAG_END, 0);
    arkos_set_tag(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    arkos_set_tag(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "arkos");
    arkos_set_tag(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    arkos_set_tag(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    arkos_set_tag(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
