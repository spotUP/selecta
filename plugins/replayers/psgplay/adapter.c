/* PSG play replayer (row I707, bbmp backend psgplay): a RENDER-kind adapter over Fredrik Noring's psgplay
 * (third_party/psgplay, GPL-2.0) for Atari ST / STE SNDH chip music. psgplay runs the SNDH file's own 68000 code on an
 * emulated ST (Musashi, YM2149, MFP 68901, STE DMA sound). It is the second SNDH backend: sc68 (plugin id sndh) stays the
 * default for .sndh through its higher SEL_RP_PRIORITY (0 against -10 here), the same mechanism that ranks kss over gme.
 * ICE!-packed files are decrunched at open (psgplay takes plain SNDH only). Built only as a MODULE under
 * SELECTA_GPL_PLUGINS. Musashi keeps the CPU in globals, so only one instance may run at a time: a second start while
 * another instance plays is refused instead of corrupting both. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include "ice/ice.h"
#include "psgplay/psgplay.h"
#include "psgplay/sndh.h"
#include "psgplay/stereo.h"

#include <stdlib.h>
#include <string.h>

#define PSG_TEXT 128u
#define PSG_CHUNK 1024u

typedef struct psg_state {
    const sel_host* host;
    uint8_t* song;
    size_t size;
    struct psgplay* pp;
    uint32_t tracks;
    uint32_t default_track; /* 1-based */
    uint32_t length_ms;
    char title[PSG_TEXT], author[PSG_TEXT];
    sel_tag meta[6];
} psg_state;

static const psg_state* g_running; /* the instance that owns Musashi's globals */

static int has_ext(const char* name, const char* ext) {
    size_t n, e = strlen(ext);
    if (name == NULL) {
        return 0;
    }
    n = strlen(name);
    if (n < e + 1u || name[n - e - 1u] != '.') {
        return 0;
    }
    for (n -= e; *ext != '\0'; ext++, n++) {
        char c = name[n];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != *ext) {
            return 0;
        }
    }
    return 1;
}

/* Same claim as the sc68 module: a plain SNDH carries "SNDH" in its header, the packed form only by extension. */
static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    size_t i;
    if (size < 16u) {
        return SEL_PROBE_NO;
    }
    if (memcmp(data, "ICE!", 4) == 0) {
        return has_ext(name, "sndh") ? SEL_PROBE_YES : SEL_PROBE_NO;
    }
    for (i = 0; i + 4u <= size && i < 64u; i++) {
        if (memcmp(data + i, "SNDH", 4) == 0) {
            return SEL_PROBE_YES;
        }
    }
    return SEL_PROBE_NO;
}

static void rp_close(void* state) {
    psg_state* s = (psg_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        if (s->pp != NULL) {
            psgplay_free(s->pp);
            if (g_running == s) {
                g_running = NULL;
            }
        }
        if (s->song != NULL) {
            host->mem_free(host, s->song);
        }
        host->mem_free(host, s);
    }
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    psg_state* s;
    int n = 1, def = 1;
    if (src->stream != NULL || src->data == NULL || src->size < 16u) {
        return NULL;
    }
    s = (psg_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    if (ice_identify(src->data, src->size)) {
        size_t out = ice_decrunched_size(src->data, src->size);
        s->song = out != 0u ? (uint8_t*)host->mem_alloc(host, out, SEL_MEM_FAST) : NULL;
        if (s->song == NULL || ice_decrunch(s->song, src->data, src->size) != (ssize_t)out) {
            rp_close(s);
            return NULL;
        }
        s->size = out;
    } else {
        s->song = (uint8_t*)host->mem_alloc(host, src->size, SEL_MEM_FAST);
        if (s->song == NULL) {
            rp_close(s);
            return NULL;
        }
        memcpy(s->song, src->data, src->size);
        s->size = src->size;
    }
    if (!sndh_identify(s->song, s->size)) {
        rp_close(s);
        return NULL;
    }
    if (!sndh_tag_subtune_count(&n, s->song, s->size) || n < 1) {
        n = 1;
    }
    if (!sndh_tag_default_subtune(&def, s->song, s->size) || def < 1 || def > n) {
        def = 1;
    }
    s->tracks = (uint32_t)n;
    s->default_track = (uint32_t)def;
    sndh_tag_title(s->title, sizeof s->title, s->song, s->size);
    sndh_tag_composer(s->author, sizeof s->author, s->song, s->size);
    info->voice_count = 0;
    info->subsong_count = s->tracks;
    info->default_subsong = s->default_track - 1u;
    info->timebase_hz = 0;
    return s;
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    psg_state* s = (psg_state*)state;
    float secs = 0.0f;
    (void)voices;
    if (rate == 0u || subsong >= s->tracks || (g_running != NULL && g_running != s)) {
        return 0;
    }
    if (s->pp != NULL) {
        psgplay_free(s->pp);
        s->pp = NULL;
        g_running = NULL;
    }
    s->pp = psgplay_init(s->song, s->size, (int)subsong + 1, (int)rate);
    if (s->pp == NULL) {
        return 0;
    }
    g_running = s;
    s->length_ms = 0;
    if (sndh_tag_subtune_time(&secs, (int)subsong + 1, s->song, s->size) && secs > 0.0f) {
        s->length_ms = (uint32_t)(secs * 1000.0f);
        psgplay_stop_at_time(s->pp, secs);
    }
    return 1;
}

static uint32_t rp_render(void* state, int16_t* out, uint32_t frames) {
    psg_state* s = (psg_state*)state;
    uint32_t done = 0;
    if (s->pp == NULL) {
        return 0;
    }
    while (done < frames) {
        size_t want = frames - done < PSG_CHUNK ? frames - done : PSG_CHUNK;
        ssize_t n = psgplay_read_stereo(s->pp, (struct psgplay_stereo*)(out + (size_t)done * 2u), want);
        if (n <= 0) {
            break;
        }
        done += (uint32_t)n;
    }
    return done;
}

static const sel_tag* rp_metadata(void* state) {
    psg_state* s = (psg_state*)state;
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
    s->meta[n++].value = (uintptr_t) "SNDH (psgplay)";
    s->meta[n].tag = SEL_META_DURATION_MS;
    s->meta[n++].value = (uintptr_t)s->length_ms;
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

const sel_tag* sel_plugin_entry(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "psgplay");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_RENDER);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sndh");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_RENDER, SEL_TAG_FN(rp_render));
    set(&g_hooks[9], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[10], SEL_RP_PRIORITY, (uintptr_t)(intptr_t)-10); /* below sc68 (0), which stays the default for .sndh */
    set(&g_hooks[11], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "psgplay");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
