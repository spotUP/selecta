/* I122/I123/I126: paula14 split tables, the chip double buffer through a recording bus, the plugin descriptors
 * and the routing table for PCM outputs (no voice backend: RENDER and any VOICE count go to the mixer). */
#include <selecta/engine.h>
#include <selecta/output.h>
#include <selecta/plugin.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../plugins/outputs/paula14/common/p14_sink.h"

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

static int g_chip_allocs;
static void* h_alloc(const sel_host* h, size_t n, uint32_t kind) {
    (void)h;
    g_chip_allocs += kind == SEL_MEM_SAMPLE;
    return calloc(1, n);
}
static void h_free(const sel_host* h, void* p) {
    (void)h;
    free(p);
}

static struct {
    uint32_t len[4], per[4], vol[4], dmacon, lc_writes;
    const void* lc[4];
} g_bus;

static void b_write(void* ctx, uint32_t reg, uint32_t value) {
    uint32_t ch = (reg - SEL_PAULA_AUD_LCH) / SEL_PAULA_CHANNEL_STRIDE;
    (void)ctx;
    if (reg == SEL_PAULA_DMACON) {
        g_bus.dmacon = value;
    } else if (ch < 4u && reg % SEL_PAULA_CHANNEL_STRIDE == SEL_PAULA_AUD_LEN % SEL_PAULA_CHANNEL_STRIDE) {
        g_bus.len[ch] = value;
    } else if (ch < 4u && reg % SEL_PAULA_CHANNEL_STRIDE == SEL_PAULA_AUD_PER % SEL_PAULA_CHANNEL_STRIDE) {
        g_bus.per[ch] = value;
    } else if (ch < 4u && reg % SEL_PAULA_CHANNEL_STRIDE == SEL_PAULA_AUD_VOL % SEL_PAULA_CHANNEL_STRIDE) {
        g_bus.vol[ch] = value;
    }
}
static void b_lc(void* ctx, uint32_t ch, const void* bytes, uint32_t offset, uint32_t serial) {
    (void)ctx;
    (void)offset;
    (void)serial;
    g_bus.lc[ch] = bytes;
    g_bus.lc_writes++;
}

int main(void) {
    sel_p14_table t;
    sel_p14 p;
    sel_host host;
    sel_paula_bus bus;
    static int16_t pcm[64 * 2];
    int8_t hi, lo;
    int32_t s;
    uint32_t i;
    const sel_tag* sh = shadow_entry(1);
    const sel_tag* hooks = (const sel_tag*)sel_tag_find(sh, SEL_PLUGIN_TAGS, 0);
    const void* first_lc;

    /* I122: every s16 input reconstructs within 1 LSB of 14 bits; extremes are exact. */
    sel_p14_table_default(&t);
    CHECK(sel_p14_max_error(&t) <= 1u, "max error %u", (unsigned)sel_p14_max_error(&t));
    for (s = -32768; s <= 32767; s += 97) {
        int32_t d;
        sel_p14_split(&t, (int16_t)s, &hi, &lo);
        CHECK(lo >= 0 && lo < 64, "lo byte %d", lo);
        d = sel_p14_merge(&t, hi, lo) - (s >> 2);
        CHECK(d >= -1 && d <= 1, "s=%d d=%d", (int)s, (int)d);
    }
    sel_p14_split(&t, 32767, &hi, &lo);
    CHECK(hi == 127 && lo == 63, "full scale %d %d", hi, lo);
    sel_p14_split(&t, -32768, &hi, &lo);
    CHECK(hi == -128 && lo == 0, "negative full scale %d %d", hi, lo);
    /* A calibration that bends one lo entry moves the error (knob test: the table is what the sink uses). */
    t.lo[10] = 13;
    CHECK(sel_p14_max_error(&t) == 3u, "calibrated error %u", (unsigned)sel_p14_max_error(&t));

    CHECK(sel_p14_period_for_rate(44100) == 0u, "44100 needs period under 113");
    CHECK(sel_p14_period_for_rate(22050) == 161u, "22050 period %u", (unsigned)sel_p14_period_for_rate(22050));

    /* I123: double buffer in chip RAM through a recording bus. */
    memset(&host, 0, sizeof host);
    host.mem_alloc = h_alloc;
    host.mem_free = h_free;
    bus.write = b_write;
    bus.set_lc = b_lc;
    bus.ctx = NULL;
    CHECK(sel_p14_open(&p, &host, &bus, 44100, 64, 1) != 0, "strict open at 44100 must fail");
    CHECK(sel_p14_open(&p, &host, &bus, 22050, 64, 1) == 0, "open");
    CHECK(g_chip_allocs == 8, "8 chip halves, got %d", g_chip_allocs);
    CHECK(g_bus.per[0] == 161u && g_bus.len[3] == 32u && g_bus.vol[0] == 64u && g_bus.vol[1] == 64u &&
              g_bus.vol[2] == 1u && g_bus.vol[3] == 1u,
          "channel setup per=%u len=%u vol=%u/%u", (unsigned)g_bus.per[0], (unsigned)g_bus.len[3],
          (unsigned)g_bus.vol[0], (unsigned)g_bus.vol[2]);
    CHECK((g_bus.dmacon & SEL_PAULA_DMA_SETCLR) != 0 && (g_bus.dmacon & 15u) == 15u, "dma on %x", (unsigned)g_bus.dmacon);
    first_lc = g_bus.lc[0];
    for (i = 0; i < 64; i++) {
        pcm[i * 2] = (int16_t)(i * 500 - 16000);
        pcm[i * 2 + 1] = (int16_t)(16000 - (int)i * 500);
    }
    sel_p14_submit(&p, pcm);
    CHECK(g_bus.lc[0] != first_lc, "first submit must schedule the other half");
    CHECK(p.half[0][1][5] == (int8_t)((pcm[10] >> 2) >> 6), "left high byte");
    CHECK(p.half[2][1][5] == t.lo[(pcm[11] >> 2) & 63], "right low byte");
    sel_p14_submit(&p, pcm);
    CHECK(g_bus.lc[0] == first_lc, "second submit must return to the first half");
    CHECK(p.bufs == 2u && p.frames == 128u, "bufs %u frames %u", (unsigned)p.bufs, (unsigned)p.frames);
    sel_p14_close(&p);

    /* I126: paula14 is a PCM output: no voice backend, so every kind and voice count goes to the mixer. */
    CHECK(sel_tag_find(hooks, SEL_OUT_VOICE_BACKEND, 0) == 0 && sel_tag_find(hooks, SEL_OUT_VOICES, 0) == 0,
          "paula14 must not offer a voice backend");
    CHECK(sel_tag_find(hooks, SEL_OUT_MEM_CHIP, 0) == 4096u, "chip need %u", (unsigned)sel_tag_find(hooks, SEL_OUT_MEM_CHIP, 0));
    CHECK(sel_tag_find(hooks, SEL_OUT_PAULA_HW, 0) == 0, "PCM output must not bypass LED or width");
    for (i = 0; i <= 32; i++) {
        uintptr_t has_sink = sel_tag_find(hooks, SEL_OUT_VOICE_BACKEND, 0) != 0;
        CHECK(sel_engine_voice_route(i, (int)has_sink, (uint32_t)sel_tag_find(hooks, SEL_OUT_VOICE_MAX, 0)) == SEL_ROUTE_MIXER,
              "voices=%u must route to the mixer", (unsigned)i);
    }
    printf("p14 failures=%d\n", failures);
    return failures != 0;
}
