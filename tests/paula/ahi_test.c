/* I124/I125 (D-M2-5): the AHI buffer length and rate per CPU class, the output.buffer override, and the shadow twin's
 * double-buffer bookkeeping through the plugin descriptor (swaps = frames / buffer length). */
#include <selecta/output.h>
#include <selecta/plugin.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../plugins/outputs/ahi/common/ahi_cfg.h"

const sel_tag* shadow_entry(uint32_t host_api);

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static const char* g_buffer_pref = "0";
static const char* cfg(const sel_host* h, const char* key, const char* def) {
    (void)h;
    return strcmp(key, "output.buffer") == 0 ? g_buffer_pref : def;
}
static void* h_alloc(const sel_host* h, size_t n, uint32_t kind) {
    (void)h;
    (void)kind;
    return calloc(1, n);
}
static void h_free(const sel_host* h, void* p) {
    (void)h;
    free(p);
}

static uint32_t g_now;
static uint32_t h_clock(const sel_host* h) {
    (void)h;
    return g_now += 50u; /* the machine probe's speed loop ends after two reads */
}

static uint32_t g_pulls;
static uint32_t pull(void* ctx, int16_t* stereo, uint32_t frames) {
    (void)ctx;
    g_pulls++;
    memset(stereo, 0, frames * 4u);
    return frames;
}

/* Pumps `total` frames through the shadow; returns the swap count before close. */
static uint32_t run_shadow(uint32_t total, uint32_t* buffer_out, uint32_t* underruns_out, uint32_t* rate_out) {
    sel_host host;
    const sel_tag* hooks = (const sel_tag*)sel_tag_find(shadow_entry(1), SEL_PLUGIN_TAGS, 0);
    sel_out_open_fn open = (sel_out_open_fn)sel_tag_find(hooks, SEL_OUT_OPEN, 0);
    sel_out_pump_fn pump = (sel_out_pump_fn)sel_tag_find(hooks, SEL_OUT_PUMP, 0);
    sel_out_set_source_fn src = (sel_out_set_source_fn)sel_tag_find(hooks, SEL_OUT_SET_SOURCE, 0);
    sel_out_stats_fn stats = (sel_out_stats_fn)sel_tag_find(hooks, SEL_OUT_STATS, 0);
    sel_out_close_fn close_fn = (sel_out_close_fn)sel_tag_find(hooks, SEL_OUT_CLOSE, 0);
    uint32_t bufs, done = 0;
    void* out;
    memset(&host, 0, sizeof host);
    host.mem_alloc = h_alloc;
    host.mem_free = h_free;
    host.config_get = cfg;
    host.clock_ms = h_clock;
    out = open(&host, 0, 2);
    if (out == NULL) {
        CHECK(0, "shadow open");
        return 0;
    }
    src(out, pull, NULL);
    while (done < total) {
        uint32_t got = pump(out, 441u);
        if (got == 0) {
            break;
        }
        done += got;
    }
    bufs = (uint32_t)sel_tag_find(stats(out), SEL_OUT_STAT_AHI_BUFS, 0);
    *buffer_out = (uint32_t)sel_tag_find(stats(out), SEL_OUT_STAT_AHI_BUFFER, 0);
    *underruns_out = (uint32_t)sel_tag_find(stats(out), SEL_OUT_STAT_AHI_UNDERRUNS, 99);
    *rate_out = (uint32_t)sel_tag_find(stats(out), SEL_OUT_STAT_AHI_RATE, 0);
    close_fn(out);
    return bufs;
}

int main(void) {
    uint32_t buffer = 0, under = 99, rate = 0, bufs;
    /* D-M2-5 table: 68000 8192, 68020 4096, 68030 2048, 68040+ 1024. */
    CHECK(sel_ahi_buffer_frames(SEL_CPU_68000) == 8192u, "68000");
    CHECK(sel_ahi_buffer_frames(SEL_CPU_68010) == 8192u, "68010");
    CHECK(sel_ahi_buffer_frames(SEL_CPU_68020) == 4096u, "68020");
    CHECK(sel_ahi_buffer_frames(SEL_CPU_68030) == 2048u, "68030");
    CHECK(sel_ahi_buffer_frames(SEL_CPU_68040) == 1024u, "68040");
    CHECK(sel_ahi_buffer_frames(SEL_CPU_68060) == 1024u, "68060");
    CHECK(sel_ahi_default_rate(SEL_CPU_68020) == 28150u, "rate 68020");
    CHECK(sel_ahi_default_rate(SEL_CPU_68040) == 44100u, "rate 68040");
    CHECK(sel_ahi_default_rate(SEL_CPU_68060) == 44100u, "rate 68060");
    /* output.buffer: nonzero in range overrides; 0, empty and out-of-range fall back to the table. */
    CHECK(sel_ahi_pick_buffer(SEL_CPU_68020, "0") == 4096u, "override 0");
    CHECK(sel_ahi_pick_buffer(SEL_CPU_68020, "") == 4096u, "override empty");
    CHECK(sel_ahi_pick_buffer(SEL_CPU_68020, "512") == 512u, "override 512");
    CHECK(sel_ahi_pick_buffer(SEL_CPU_68020, "32") == 4096u, "override too small");
    CHECK(sel_ahi_pick_buffer(SEL_CPU_68020, "99999999") == 4096u, "override too large");

    /* The shadow runs on the desktop CPU class: 1024 frames, 44100 Hz. 44100 frames = 43 swaps (68 frames flushed). */
    bufs = run_shadow(44100u, &buffer, &under, &rate);
    CHECK(bufs == 43u && buffer == 1024u && under == 0u && rate == 44100u, "default: bufs %u buffer %u under %u rate %u",
          bufs, buffer, under, rate);
    CHECK(g_pulls > 0u, "pull sentinel");
    g_buffer_pref = "441";
    bufs = run_shadow(44100u, &buffer, &under, &rate);
    CHECK(bufs == 100u && buffer == 441u, "override: bufs %u buffer %u", bufs, buffer);
    printf(failures ? "ahi_test FAILED\n" : "ahi_test OK\n");
    return failures != 0;
}
