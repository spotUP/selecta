/* Fixture module (I434j3): HW-kind replayer "sidfeed" claiming *.selsid files (magic "SELSID"). It plays nothing; it only
 * answers SEL_RP_VIZ_SID with a known one-chip register pattern (Voice 1 gate on, frequency $1000, pulse $0800, sustain
 * 8, filter on, volume 15) and counts the calls, so the engine's HW viz path fills sid_chips like the playsid adapter. */
#include <selecta/plugin.h>
#include <selecta/replayer.h>

#include <stdlib.h>
#include <string.h>

typedef struct fx_state {
    const sel_host* host;
    int playing;
} fx_state;

static int rp_probe(const uint8_t* data, size_t size, const char* name) {
    (void)name;
    return size >= 6u && memcmp(data, "SELSID", 6) == 0 ? SEL_PROBE_YES : SEL_PROBE_NO;
}

static void* rp_open(const sel_song_source* src, const sel_host* host, sel_rp_info* info) {
    fx_state* s;
    if (src->data == NULL || rp_probe(src->data, src->size, "") != SEL_PROBE_YES) {
        return NULL;
    }
    s = (fx_state*)host->mem_alloc(host, sizeof *s, SEL_MEM_FAST);
    if (s == NULL) {
        return NULL;
    }
    memset(s, 0, sizeof *s);
    s->host = host;
    info->voice_count = 0;
    info->subsong_count = 1;
    info->default_subsong = 0;
    info->timebase_hz = 0;
    return s;
}

static void rp_close(void* state) {
    fx_state* s = (fx_state*)state;
    if (s != NULL) {
        s->host->mem_free(s->host, s);
    }
}

static int rp_start(void* state, uint32_t subsong, uint32_t rate, const sel_voices* voices) {
    fx_state* s = (fx_state*)state;
    (void)voices;
    if (rate == 0u || subsong != 0u) {
        return 0;
    }
    s->playing = 1;
    return 1;
}

static void rp_hw_pause(void* state, uint32_t paused) {
    (void)state;
    (void)paused;
}

static void rp_hw_release(void* state) {
    ((fx_state*)state)->playing = 0;
}

static uint32_t rp_hw_poll(void* state) {
    (void)state;
    return 0u; /* the tune never ends */
}

static uint32_t rp_viz_sid(void* state, uint8_t* regs, uint8_t* env, uint32_t max_chips) {
    fx_state* s = (fx_state*)state;
    if (!s->playing || max_chips < 1u) {
        return 0;
    }
    regs[0] = 0x00; /* Voice 1 frequency $1000 */
    regs[1] = 0x10;
    regs[2] = 0x00; /* pulse $0800 */
    regs[3] = 0x08;
    regs[4] = 0x41; /* pulse waveform, gate on */
    regs[5] = 0x08; /* attack, decay */
    regs[6] = 0x88; /* sustain, release */
    regs[23] = 0x01; /* filter voice 1 */
    regs[24] = 0x1F; /* volume 15, low pass */
    env[0] = 0x80;
    return 1;
}

static sel_tag g_hooks[13];
static sel_tag g_desc[5];

static void set(sel_tag* t, uint32_t tag, uintptr_t value) {
    t->tag = tag;
    t->value = value;
}

const sel_tag* sel_plugin_entry(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    set(&g_hooks[0], SEL_RP_NAME, (uintptr_t) "sidfeed");
    set(&g_hooks[1], SEL_RP_VERSION, 0x00010000);
    set(&g_hooks[2], SEL_RP_KIND, SEL_RP_KIND_HW);
    set(&g_hooks[3], SEL_RP_EXTENSIONS, (uintptr_t) "selsid");
    set(&g_hooks[4], SEL_RP_PROBE, SEL_TAG_FN(rp_probe));
    set(&g_hooks[5], SEL_RP_OPEN, SEL_TAG_FN(rp_open));
    set(&g_hooks[6], SEL_RP_CLOSE, SEL_TAG_FN(rp_close));
    set(&g_hooks[7], SEL_RP_START, SEL_TAG_FN(rp_start));
    set(&g_hooks[8], SEL_RP_HW_PAUSE, SEL_TAG_FN(rp_hw_pause));
    set(&g_hooks[9], SEL_RP_HW_RELEASE, SEL_TAG_FN(rp_hw_release));
    set(&g_hooks[10], SEL_RP_VIZ_SID, SEL_TAG_FN(rp_viz_sid));
    set(&g_hooks[11], SEL_RP_HW_POLL, SEL_TAG_FN(rp_hw_poll));
    set(&g_hooks[12], SEL_TAG_END, 0);

    set(&g_desc[0], SEL_PLUGIN_KIND, SEL_PLUGIN_KIND_REPLAYER);
    set(&g_desc[1], SEL_PLUGIN_ID, (uintptr_t) "sidfeed");
    set(&g_desc[2], SEL_PLUGIN_VERSION, 1);
    set(&g_desc[3], SEL_PLUGIN_TAGS, (uintptr_t)g_hooks);
    set(&g_desc[4], SEL_TAG_END, 0);
    return g_desc;
}
