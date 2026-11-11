/* Hively replayer (row I1242j): a RENDER-kind adapter over the HivelyTracker replayer (third_party/hively, BSD 3-Clause,
 * Pete Gordon) for AHX / THX songs (Abyss' Highest eXperience, Amiga) and Hively (.hvl) songs. The module claims `ahx`,
 * `thx` and `hvl`; the engine matches those against the last extension (`song.ahx`) and the Amiga prefix (`ahx.song`,
 * the Modland spelling, src/engine/claims.c), and the probe decides on the header ("THX" + version 0..2, "HVL" + 0..1).
 * The song plays until the replayer reports the end of the song (ht_SongEndReached, set when the last position has
 * played), capped at HVL_MAX_SECONDS for songs that loop for ever. The replayer mixes a 50 Hz frame at a time, so the
 * adapter keeps one decoded chunk and serves it across render calls. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "hvl_replay.h"
#include "probe.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HVL_MAX_SIZE (4u * 1024u * 1024u)
/* The replayer trusts the header counts: the largest claim (4095 positions, 255 tracks of 64 steps, 255 instruments with
 * 255 list entries) stays below this, so zero padding keeps every read inside the copy. */
#define HVL_PAD (600u * 1024u)
#define HVL_MAX_SECONDS 1200u
#define HVL_CHUNK_FRAMES 4096u /* freq / 50 at the highest rate we accept (192 kHz) */
#define HVL_MIN_RATE 8000u
#define HVL_MAX_RATE 192000u

typedef struct hvl_state {
    const sel_host* host;
    uint8_t* data; /* the song plus HVL_PAD zero bytes */
    size_t size;
    struct hvl_tune* ht;
    uint32_t rate;
    uint32_t subsongs;
    uint32_t chunk_frames; /* frames one hvl_DecodeFrame produces */
    uint32_t chunk_pos;
    uint32_t chunk_len;
    uint64_t frames;
    uint64_t max_frames;
    int ended;
    int16_t chunk[HVL_CHUNK_FRAMES * 2u];
    sel_tag meta[4];
} hvl_state;

static void tune_free(hvl_state* s) {
    if (s->ht != NULL) {
        hvl_FreeTune(s->ht);
        s->ht = NULL;
    }
}

static void rp_close(void* state) {
    hvl_state* s = (hvl_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        tune_free(s);
        host->mem_free(host, s->data);
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    hvl_state* s;
    if (src->stream != NULL || src->data == NULL || src->size < HVL_MIN_SIZE || src->size > HVL_MAX_SIZE
        || hvl_probe(src->data, src->size, NULL) == SEL_PROBE_NO) {
        return NULL;
    }
    s = (hvl_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = (uint8_t*)host->mem_alloc(host, src->size + HVL_PAD, SEL_MEM_FAST);
    if (s->data == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    memcpy(s->data, src->data, src->size);
    memset(s->data + src->size, 0, HVL_PAD);
    s->size = src->size;
    /* Parse once to learn the subsong count and to reject a file the replayer refuses; rp_start parses again at the
     * session rate (the tune holds the rate). */
    hvl_InitReplayer();
    s->ht = hvl_ParseTune(s->data, (uint32_t)s->size, 44100u, 0u);
    if (s->ht == NULL) {
        rp_close(s);
        return NULL;
    }
    s->subsongs = (uint32_t)s->ht->ht_SubsongNr + 1u;
    tune_free(s);
    info->voice_count = 0;
    info->subsong_count = s->subsongs;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    hvl_state* s = (hvl_state*)state;
    uint32_t mult;
    (void)voices;
    if (rate < HVL_MIN_RATE || rate > HVL_MAX_RATE || subsong >= s->subsongs) {
        return 0;
    }
    tune_free(s);
    s->ht = hvl_ParseTune(s->data, (uint32_t)s->size, rate, 0u);
    if (s->ht == NULL || !hvl_InitSubsong(s->ht, subsong)) {
        tune_free(s);
        return 0;
    }
    mult = s->ht->ht_SpeedMultiplier != 0u ? s->ht->ht_SpeedMultiplier : 1u;
    s->rate = rate;
    s->chunk_frames = (rate / 50u / mult) * mult;
    s->chunk_pos = 0;
    s->chunk_len = 0;
    s->frames = 0;
    s->max_frames = (uint64_t)rate * HVL_MAX_SECONDS;
    s->ended = 0;
    return s->chunk_frames != 0u && s->chunk_frames <= HVL_CHUNK_FRAMES;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    hvl_state* s = (hvl_state*)state;
    uint32_t done = 0;
    if (s->ht == NULL) {
        return 0;
    }
    while (done < frames && !s->ended) {
        uint32_t n;
        if (s->chunk_pos >= s->chunk_len) {
            if (s->ht->ht_SongEndReached || s->frames >= s->max_frames) {
                s->ended = 1;
                break;
            }
            hvl_DecodeFrame(s->ht, (int8*)&s->chunk[0], (int8*)&s->chunk[1], 4);
            s->chunk_pos = 0;
            s->chunk_len = s->chunk_frames;
        }
        n = s->chunk_len - s->chunk_pos;
        if (n > frames - done) {
            n = frames - done;
        }
        memcpy(out + (size_t)done * 2u, s->chunk + (size_t)s->chunk_pos * 2u, (size_t)n * 4u);
        s->chunk_pos += n;
        s->frames += n;
        done += n;
    }
    return done;
}

static int rp_seek(void* state, uint32_t ms) {
    hvl_state* s = (hvl_state*)state;
    int16_t scratch[2048 * 2];
    uint64_t left = (uint64_t)ms * s->rate / 1000u;
    uint32_t subsong;
    if (s->ht == NULL) {
        return 0;
    }
    subsong = s->ht->ht_SongNum;
    if (!rp_start(s, subsong, s->rate, NULL)) {
        return 0;
    }
    /* Voices carry state (envelopes, list positions): run the song forward and discard the output. */
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
    hvl_state* s = (hvl_state*)state;
    uint32_t n = 0;
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t)(s->data[0] == 'T' ? "AHX" : "Hively");
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
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
    {
        size_t n = hvl_static_hooks(g_hooks); /* probe.c: shared with the Amiga manifest (I427e4) */
        set(&g_hooks[n++], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
        set(&g_hooks[n++], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
        set(&g_hooks[n++], SEL_RP_START, SEL_TAG_FN(rp_start));
        set(&g_hooks[n++], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
        set(&g_hooks[n++], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
        set(&g_hooks[n++], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
        set(&g_hooks[n], SEL_TAG_END, 0);
    }

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "hively");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
