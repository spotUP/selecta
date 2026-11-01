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
    uint32_t oversample; /* SetRESIDMode argument (reSID modes) */
    uint32_t playing;
    uint32_t length_ms;  /* catalogue (HVSC) length of the started subsong, 0 = unknown (RP_HW_LENGTH) */
    uint32_t run_ms;     /* play time accumulated before t_mark */
    uint32_t t_mark;     /* host clock at the last start or resume */
    uint32_t paused;
    uint32_t ended;      /* RP_HW_POLL already answered SEL_TICK_ENDED for this start */
    char title[PS_TEXT], author[PS_TEXT], release[PS_TEXT];
    const char* format; /* info label: "SID", "SID 2SID", "SID 3SID" */
    sel_tag meta[6];
} ps_state;

static ps_state* g_owner;

/* Sentinels (tests/engine/playsid_test.c): the adapter ran, per hook. */
uint32_t sel_playsid_probes, sel_playsid_starts, sel_playsid_seeks, sel_playsid_ends, sel_playsid_viz_fills, sel_playsid_perf_gates, sel_playsid_perf_measures, sel_playsid_perf_asks, sel_playsid_reg_fills;

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

static const char* cfg(const sel_host* host, const char* key, const char* def) {
    return host->config_get != NULL ? host->config_get(host, key, def) : def;
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
    {
        uint32_t chips = ps_sid_count(s->image, s->size);
        s->format = chips == 3u ? "SID 3SID" : chips == 2u ? "SID 2SID" : "SID";
    }
    copy_text(s->title, s->image + 22);
    copy_text(s->author, s->image + 54);
    copy_text(s->release, s->image + 86);
    s->mode = ps_mode_from_pref(cfg(host, "playsid.mode", "normal")); /* so RP_SEEKABLE answers before the first start */
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
    s->meta[n++].value = (uintptr_t)s->format;
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

/* Hippo (puu016.s .sanko) picks 50 or 60 Hz from the header speed word with btst on the 1-based song number. */
static uint32_t vert_freq(const ps_state* s, uint32_t song1) {
    return (s->speed_mask >> (song1 & 31u)) & 1u ? 60u : 50u;
}

/* I434h: the reSID speed gate (HippoPlayer .performanceCheck). The library benchmark runs once per (oversample, internal
 * filter, external filter) and the measured time is cached; the limit scales with the SID count, so a 3SID tune is
 * judged against a third of the one-SID reference. Too slow: the host asks "Continue anyway / Stop" through
 * SEL_HOSTQ_CONFIRM; a host without a dialog lets the song start. The answer Continue is cached per combination, so a
 * tune is not asked about twice. Returns 1 = start, 0 = the user stopped. */
#define PS_PERF_SLOTS 16u
typedef struct ps_perf {
    uint32_t key, measured, limit, accepted_sids; /* accepted_sids: bit n set = "continue" given for n SIDs */
} ps_perf;
static ps_perf g_perf[PS_PERF_SLOTS];
static uint32_t g_perf_n;

static int perf_gate(ps_state* s, uint32_t in_on, uint32_t ext_on) {
    uint32_t key = (s->oversample << 8) | (in_on << 4) | ext_on, i, sids, limit;
    ps_perf* p = NULL;
    sel_tag ask, ans;
    sel_playsid_perf_gates++;
    for (i = 0; i < g_perf_n; i++) {
        if (g_perf[i].key == key) {
            p = &g_perf[i];
        }
    }
    if (p == NULL) {
        uint32_t one = 0;
        uint32_t ms = s->ops->measure_resid(s->oversample, in_on, ext_on, &one);
        sel_playsid_perf_measures++;
        if (g_perf_n == PS_PERF_SLOTS) {
            g_perf_n = 0;
        }
        p = &g_perf[g_perf_n++];
        p->key = key;
        p->measured = ms;
        p->limit = one;
        p->accepted_sids = 0;
    }
    sids = ps_sid_count(s->image, s->size);
    limit = ps_perf_limit(p->limit, sids);
    if (p->measured <= limit || (p->accepted_sids >> sids & 1u) != 0u) {
        return 1;
    }
    sel_playsid_perf_asks++;
    ask.tag = SEL_HOSTQ_CONFIRM;
    ask.value = (uintptr_t) "Your Amiga could be too slow for this SID mode! Continue anyway, or Stop?";
    ans.tag = SEL_TAG_END;
    ans.value = 1;
    if (s->host->query != NULL && s->host->query(s->host, &ask, &ans) && ans.tag == SEL_HOSTQ_CONFIRM && ans.value == 0) {
        return 0;
    }
    p->accepted_sids |= 1u << sids;
    return 1;
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
    if (ps_mode_is_resid(s->mode)) {
        s->oversample = ps_oversample_from_pref(cfg(s->host, "playsid.oversample", "normal"));
        s->ops->set_resid_mode(s->oversample);
    }
    if (ps_mode_is_resid(s->mode)) {
        uint32_t in_on, ext_on;
        ps_filter_from_pref(cfg(s->host, "playsid.filter", "internal"), &in_on, &ext_on);
        if (!perf_gate(s, in_on, ext_on)) {
            return 0;
        }
    }
    if (s->ops->alloc_emul() != 0) {
        return 0;
    }
    if (ps_mode_is_resid(s->mode)) { /* filter and boost apply after AllocEmulResource, as in puu016.s */
        uint32_t in_on, ext_on;
        ps_filter_from_pref(cfg(s->host, "playsid.filter", "internal"), &in_on, &ext_on);
        s->ops->set_resid_filter(in_on, ext_on);
        s->ops->set_resid_boost(ps_boost_from_pref(cfg(s->host, "playsid.boost", "off")));
    }
    s->ops->set_module(s->image, s->image, (uint32_t)s->size);
    s->ops->set_vert_freq(vert_freq(s, song1));
    if (s->ops->start_song(song1) != 0) {
        s->ops->free_emul();
        return 0;
    }
    s->playing = 1;
    s->length_ms = 0; /* the engine sets it through RP_HW_LENGTH right after this start */
    s->run_ms = 0;
    s->t_mark = s->host->clock_ms(s->host);
    s->paused = 0;
    s->ended = 0;
    g_owner = s;
    sel_playsid_starts++;
    return 1;
}

/* Play time of the running subsong: the library plays on its own clock, so the adapter counts host time and holds it
 * while paused. */
static uint32_t elapsed_ms(const ps_state* s) {
    return s->paused ? s->run_ms : s->run_ms + (s->host->clock_ms(s->host) - s->t_mark);
}

static void rp_hw_length(void* state, uint32_t ms) {
    ((ps_state*)state)->length_ms = ms;
}

/* The song ends when the catalogue length has passed; an unknown length (0) never ends it here, the engine's
 * max_song_ms still does. */
static uint32_t rp_hw_poll(void* state) {
    ps_state* s = (ps_state*)state;
    if (!s->playing || s->ended || s->length_ms == 0u || elapsed_ms(s) < s->length_ms) {
        return 0;
    }
    s->ended = 1;
    sel_playsid_ends++;
    return SEL_TICK_ENDED;
}

/* Scope feed (I434g): the reSID modes render into per-SID signed 8-bit buffers that GetResidAudioBuffer exposes; the
 * newest samples go last into each voice row, scaled to 16 bit. Classic mode has no buffer: 0 voices, the scope stays
 * in its empty state. The voice count is the SID count of the file. */
static uint32_t rp_viz_voices(void* state, int16_t* out, uint32_t stride, uint32_t max_voices) {
    ps_state* s = (ps_state*)state;
    const int8_t* bufs[3] = {NULL, NULL, NULL};
    uint32_t period = 0, len, k, i, n, sids;
    if (!s->playing || s->paused || !ps_mode_is_resid(s->mode)) {
        return 0;
    }
    len = s->ops->get_resid_buffer(bufs, &period);
    sids = ps_sid_count(s->image, s->size);
    if (sids > max_voices) sids = max_voices;
    if (len == 0u || bufs[0] == NULL) {
        return 0;
    }
    n = len < stride ? len : stride;
    for (k = 0; k < sids; k++) {
        const int8_t* b = bufs[k] != NULL ? bufs[k] : bufs[0];
        int16_t* row = out + (size_t)k * stride;
        memset(row, 0, (size_t)(stride - n) * sizeof *row);
        for (i = 0; i < n; i++) {
            row[stride - n + i] = (int16_t)((int32_t)b[len - n + i] * 256);
        }
    }
    sel_playsid_viz_fills++;
    return sids;
}

/* Register feed (I434j): the library's C64 memory mirror, every operating mode (the 6502 emulation writes it in
 * classic and reSID alike). The chip count is the SID count of the file. */
static uint32_t rp_viz_sid(void* state, uint8_t* regs, uint8_t* env, uint32_t max_chips) {
    ps_state* s = (ps_state*)state;
    uint32_t chips;
    if (!s->playing || s->paused) {
        return 0;
    }
    chips = ps_sid_count(s->image, s->size);
    if (chips > max_chips) chips = max_chips;
    chips = s->ops->read_sid_regs(regs, env, chips);
    if (chips > 0u) sel_playsid_reg_fills++;
    return chips;
}

static void rp_hw_pause(void* state, uint32_t paused) {
    ps_state* s = (ps_state*)state;
    if (s->playing && paused != s->paused) {
        if (paused) {
            s->run_ms = elapsed_ms(s);
        } else {
            s->t_mark = s->host->clock_ms(s->host);
        }
        s->paused = paused;
    }
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

/* Seeking exists only in classic mode (ps_mode_seekable); reSID and hardware modes answer 0 and RP_SEEKABLE 0, so the
 * UI greys the bar. The engine restarts the subsong before RP_SEEK, so the target is that many ForwardSong calls from
 * the start (ps_forward_steps). */
static int rp_seekable(void* state) {
    return ps_mode_seekable(((ps_state*)state)->mode);
}

static int rp_seek(void* state, uint32_t ms) {
    ps_state* s = (ps_state*)state;
    uint32_t n;
    if (!s->playing || g_owner != s || !ps_mode_seekable(s->mode)) {
        return 0;
    }
    s->run_ms = ms; /* the engine restarted the subsong: the target is the new play time */
    s->t_mark = s->host->clock_ms(s->host);
    for (n = ps_forward_steps(ms); n > 0u; n--) {
        s->ops->forward_song();
        sel_playsid_seeks++;
    }
    return 1;
}

static sel_tag g_hooks[20];
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
    set(&g_hooks[14], SEL_RP_SEEKABLE, SEL_TAG_FN(rp_seekable));
    set(&g_hooks[15], SEL_RP_HW_POLL, SEL_TAG_FN(rp_hw_poll));
    set(&g_hooks[16], SEL_RP_HW_LENGTH, SEL_TAG_FN(rp_hw_length));
    set(&g_hooks[17], SEL_RP_VIZ_VOICES, SEL_TAG_FN(rp_viz_voices));
    set(&g_hooks[18], SEL_RP_VIZ_SID, SEL_TAG_FN(rp_viz_sid));
    set(&g_hooks[19], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "playsid");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
