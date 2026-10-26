/* Game Music Emu replayer (design doc "Format coverage rule", row I426): a RENDER-kind adapter over libgme 0.6.5
 * (third_party/gme, LGPL 2.1). One plugin for the emulator family: AY (ZX Spectrum, Amstrad CPC), GBS (Game Boy),
 * GYM (Mega Drive), HES (PC Engine), KSS (MSX), NSF/NSFE (NES), SAP (Atari POKEY) and SPC (SNES). VGM/VGZ stays with
 * the libvgm replayer (I431). libgme identifies the format from the file header (gme_identify_header), renders
 * interleaved stereo int16 at the host rate and reports the track list of multi-song files as subsongs. A song
 * with no stored length plays the library's default length (150 s) or the intro plus two loops, then ends. */
#include <gme.h>

#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

#define GME_TEXT 256u
#define GME_OPEN_RATE 44100u

typedef struct gme_state {
    const sel_host* host;
    Music_Emu* emu;
    const uint8_t* data;
    size_t size;
    uint32_t rate;
    uint32_t tracks;
    uint32_t subsong;
    uint64_t played;      /* frames rendered since start */
    uint64_t limit;       /* frames until the song is over */
    uint32_t length_ms;   /* track length as libgme computed it */
    char title[GME_TEXT], author[GME_TEXT], system[GME_TEXT], message[GME_TEXT];
    sel_tag meta[8];
} gme_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    const char* type;
    (void)name;
    if (size < 4u) {
        return SEL_PROBE_NO;
    }
    type = gme_identify_header(data);
    return type != NULL && type[0] != '\0' ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void copy_text(char* dst, const char* src) {
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, GME_TEXT - 1u);
    dst[GME_TEXT - 1u] = '\0';
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    gme_state* s;
    if (src->stream != NULL || src->data == NULL) {
        return NULL;
    }
    s = (gme_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->data = src->data;
    s->size = src->size;
    s->rate = GME_OPEN_RATE;
    /* libgme fixes the sample rate at open; rp_start reopens the same bytes when the host rate differs. */
    if (gme_open_data(src->data, (long)src->size, &s->emu, (int)s->rate) != NULL || s->emu == NULL) {
        host->mem_free(host, s);
        return NULL;
    }
    s->tracks = (uint32_t)gme_track_count(s->emu);
    info->voice_count = 0;
    info->subsong_count = s->tracks != 0u ? s->tracks : 1u;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    gme_state* s = (gme_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        gme_delete(s->emu);
        host->mem_free(host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    gme_state* s = (gme_state*)state;
    gme_info_t* gi = NULL;
    (void)voices;
    if (rate == 0u || subsong >= (s->tracks != 0u ? s->tracks : 1u)) {
        return 0;
    }
    if (rate != s->rate) {
        Music_Emu* fresh = NULL;
        if (gme_open_data(s->data, (long)s->size, &fresh, (int)rate) != NULL || fresh == NULL) {
            return 0;
        }
        gme_delete(s->emu);
        s->emu = fresh;
        s->rate = rate;
    }
    if (gme_start_track(s->emu, (int)subsong) != NULL) {
        return 0;
    }
    s->subsong = subsong;
    s->played = 0;
    s->length_ms = 0;
    s->title[0] = s->author[0] = s->system[0] = s->message[0] = '\0';
    if (gme_track_info(s->emu, &gi, (int)subsong) == NULL && gi != NULL) {
        s->length_ms = gi->play_length > 0 ? (uint32_t)gi->play_length : 0u;
        copy_text(s->title, gi->song);
        copy_text(s->author, gi->author);
        copy_text(s->system, gi->system);
        copy_text(s->message, gi->comment);
        gme_free_info(gi);
    }
    s->limit = (uint64_t)s->length_ms * rate / 1000u;
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    gme_state* s = (gme_state*)state;
    if (s->limit != 0u) {
        uint64_t left = s->limit > s->played ? s->limit - s->played : 0u;
        if (left == 0u) {
            return 0;
        }
        if (frames > left) {
            frames = (uint32_t)left;
        }
    }
    if (gme_play(s->emu, (int)(frames * 2u), out) != NULL) {
        return 0;
    }
    s->played += frames;
    return frames;
}

static int rp_seek(void* state, uint32_t ms) {
    gme_state* s = (gme_state*)state;
    if (gme_seek(s->emu, (int)ms) != NULL) {
        return 0;
    }
    s->played = (uint64_t)ms * s->rate / 1000u;
    return 1;
}

static const sel_tag* rp_metadata(void* state) {
    gme_state* s = (gme_state*)state;
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
    s->meta[n++].value = (uintptr_t)(s->system[0] != '\0' ? s->system : "Game Music Emu");
    s->meta[n].tag = SEL_META_SUBSONGS;
    s->meta[n++].value = (uintptr_t)(s->tracks != 0u ? s->tracks : 1u);
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->length_ms;
    if (s->message[0] != '\0') {
        s->meta[n].tag = SEL_META_MESSAGE;
        s->meta[n++].value = (uintptr_t)s->message;
    }
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static sel_tag g_hooks[12];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "gme");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "ay,gbs,gym,hes,kss,nsf,nsfe,sap,spc");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[10], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "gme");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
