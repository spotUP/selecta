/* Native playsid.library adapter (I434b, design: thoughts/shared/research/2026-11-01-playsid-inventory.md groups 0, 1,
 * 4). HW kind: the library plays PSID/RSID through its own audio path (classic cycle-exact or reSID, optionally on
 * SIDBlaster, ZorroSID, USBSID-Pico or Trinity hardware), so the engine hands the output over (SEL_ROUTE_HANDOVER)
 * and only counts time, as for kplayer. The library is not bundled: it is opened from LIBS:, and rp_probe answers NO
 * when it is absent so the registry falls through to the next SID owner (nuade on the Amiga, libsidplayfp on hosts).
 * The library is reached only through the ps_ops table (playsid_api.h) so the host test substitutes a stub.
 * Only one song plays at a time: g_owner tracks the instance that holds the library. */
#include <selecta/host.h>
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <string.h>

#include "playsid_api.h"

#define PS_HEADER_MIN 0x76u
#define PS_TEXT 33u

typedef struct ps_state {
    const sel_host* host;
    const ps_ops* ops;
    uint8_t* image; /* copy of the file; the library reads it while the song plays */
    size_t size;
    uint32_t subsongs, default_subsong, speed_mask;
    uint32_t mode;       /* PS_OM_* chosen at start */
    uint32_t ahi_mode;   /* SetAHIMode argument chosen at start */
    uint32_t playing;
    char title[PS_TEXT], author[PS_TEXT], release[PS_TEXT];
    sel_tag meta[6];
} ps_state;

static ps_state* g_owner;

/* Sentinels (tests/engine/playsid_test.c): the adapter ran, per hook. */
uint32_t sel_playsid_probes, sel_playsid_starts, sel_playsid_seeks;

static uint32_t be16(const uint8_t* p) {
    return ((uint32_t)p[0] << 8) | p[1];
}

static uint32_t be32(const uint8_t* p) {
    return (be16(p) << 16) | be16(p + 2);
}

static int has_magic(const uint8_t* data, size_t size) {
    return data != NULL && size >= PS_HEADER_MIN && (memcmp(data, "PSID", 4) == 0 || memcmp(data, "RSID", 4) == 0);
}

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    const ps_ops* ops;
    (void)name;
    sel_playsid_probes++;
    if (!has_magic(data, size)) {
        return SEL_PROBE_NO;
    }
    ops = ps_ops_get();
    if (!ops->open()) {
        return SEL_PROBE_NO; /* library not installed: the next SID owner takes the file */
    }
    ops->close();
    return SEL_PROBE_YES;
}

static void copy_text(char* dst, const uint8_t* src) {
    memcpy(dst, src, PS_TEXT - 1u);
    dst[PS_TEXT - 1u] = '\0';
}

static void stop(ps_state* s) {
    if (s->playing && g_owner == s) {
        s->ops->stop_song();
        s->ops->free_emul();
        g_owner = NULL;
    }
    s->playing = 0;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    ps_state* s;
    const ps_ops* ops;
    if (src == NULL || src->stream != NULL || !has_magic(src->data, src->size)) {
        return NULL;
    }
    ops = ps_ops_get();
    if (!ops->open()) {
        return NULL;
    }
    s = (ps_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        ops->close();
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    s->ops = ops;
    s->image = (uint8_t*)host->mem_alloc(host, src->size, SEL_MEM_FAST);
    if (s->image == NULL) {
        host->mem_free(host, s);
        ops->close();
        return NULL;
    }
    memcpy(s->image, src->data, src->size);
    s->size = src->size;
    s->subsongs = be16(s->image + 14);
    s->default_subsong = be16(s->image + 16);
    s->speed_mask = be32(s->image + 18);
    if (s->subsongs == 0u) {
        s->subsongs = 1u;
    }
    s->default_subsong = s->default_subsong > 0u && s->default_subsong <= s->subsongs ? s->default_subsong - 1u : 0u;
    copy_text(s->title, s->image + 22);
    copy_text(s->author, s->image + 54);
    copy_text(s->release, s->image + 86);
    info->voice_count = 0; /* the library owns the voices */
    info->subsong_count = s->subsongs;
    info->default_subsong = s->default_subsong;
    return s;
}

static const sel_tag* rp_metadata(void* state) {
    ps_state* s = (ps_state*)state;
    uint32_t n = 0;
    if (s->title[0] != '\0') {
        s->meta[n].tag = SEL_META_TITLE;
        s->meta[n++].value = (uintptr_t)s->title;
    }
    if (s->author[0] != '\0') {
        s->meta[n].tag = SEL_META_AUTHOR;
        s->meta[n++].value = (uintptr_t)s->author;
    }
    if (s->release[0] != '\0') {
        s->meta[n].tag = SEL_META_RELEASE;
        s->meta[n++].value = (uintptr_t)s->release;
    }
    s->meta[n].tag = SEL_META_FORMAT;
    s->meta[n++].value = (uintptr_t) "SID";
    s->meta[n].tag = SEL_META_SUBSONGS;
    s->meta[n++].value = (uintptr_t)s->subsongs;
    s->meta[n].tag = SEL_TAG_END;
    s->meta[n].value = 0;
    return s->meta;
}

static void rp_close(void* state) {
    ps_state* s = (ps_state*)state;
    if (s != NULL) {
        const sel_host* host = s->host;
        stop(s);
        s->ops->close();
        host->mem_free(host, s->image);
        host->mem_free(host, s);
    }
}

static const char* cfg(const sel_host* host, const char* key, const char* def) {
    return host->config_get != NULL ? host->config_get(host, key, def) : def;
}

/* Hippo (puu016.s .sanko) picks 50 or 60 Hz from the header speed word with btst on the 1-based song number. */
static uint32_t vert_freq(const ps_state* s, uint32_t song1) {
    return (s->speed_mask >> (song1 & 31u)) & 1u ? 60u : 50u;
}

/* Order as in puu016.s: SetOperatingMode, SetAHIMode, AllocEmulResource, SetModule, SetVertFreq, StartSong. A subsong
 * change comes through here again: the song is stopped and the resource given back first. */
static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    ps_state* s = (ps_state*)state;
    uint32_t song1 = subsong < s->subsongs ? subsong + 1u : s->default_subsong + 1u;
    (void)rate;
    (void)voices;
    stop(s);
    if (g_owner != NULL) {
        stop(g_owner);
    }
    s->mode = ps_mode_from_pref(cfg(s->host, "playsid.mode", "normal"));
    s->ahi_mode = ps_ahi_mode_for_output(cfg(s->host, "output.id", ""), cfg(s->host, "playsid.ahi_mode", ""));
    s->ops->set_operating_mode(s->mode);
    s->ops->set_ahi_mode(s->ahi_mode);
    if (s->ops->alloc_emul() != 0) {
        return 0;
    }
    s->ops->set_module(s->image, s->image, (uint32_t)s->size);
    s->ops->set_vert_freq(vert_freq(s, song1));
    if (s->ops->start_song(song1) != 0) {
        s->ops->free_emul();
        return 0;
    }
    s->playing = 1;
    g_owner = s;
    sel_playsid_starts++;
    return 1;
}

static void rp_hw_pause(void* state, uint32_t paused) {
    ps_state* s = (ps_state*)state;
    if (s->playing && g_owner == s) {
        if (paused) {
            s->ops->pause_song();
        } else {
            s->ops->continue_song();
        }
    }
}

static void rp_hw_release(void* state) {
    stop((ps_state*)state);
}

/* Fast forward is the library's ForwardSong and only exists in classic mode (reSID and hardware modes return 0, the
 * engine falls back to restart-and-skip). Seek rules and UI greying are I434e. */
static int rp_seek(void* state, uint32_t ms) {
    ps_state* s = (ps_state*)state;
    (void)ms;
    if (!s->playing || g_owner != s || s->mode != PS_OM_NORMAL) {
        return 0;
    }
    s->ops->forward_song();
    sel_playsid_seeks++;
    return 1;
}

static sel_tag g_hooks[15];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "playsid");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_HW);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "sid,psid");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_HW_PAUSE, SEL_TAG_FN(rp_hw_pause));
    set(&g_hooks[9], SEL_RP_HW_RELEASE, SEL_TAG_FN(rp_hw_release));
    set(&g_hooks[10], SEL_RP_MIN_CPU, 68020);
    set(&g_hooks[11], SEL_RP_PRIORITY, 20); /* above nuade and libsidplayfp (priority 0) */
    set(&g_hooks[12], SEL_RP_SEEK, SEL_TAG_FN(rp_seek));
    set(&g_hooks[13], SEL_RP_METADATA, SEL_TAG_FN(rp_metadata));
    set(&g_hooks[14], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "playsid");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
