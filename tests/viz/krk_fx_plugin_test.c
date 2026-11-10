/* I1052b: reachability of the Amiga tier (KRK_BACKEND_FX build of plugins/viz/krakatoa/krakatoa.c) through the plugin
 * entry and its VIZ_FRAME hook, the way desktop_ui.c drives it. Proves the fixed-point core ran (stats sentinel
 * "gpu 2", krk_fx frames counted through the stats steps), that the picture is not flat, and that the palette scheduler's
 * crossfade reaches the pixels: after key '3' the same sim state is drawn with a different ramp mid-fade, then settles on
 * the ramp krk_fx_palette bakes for palette 2. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <selecta/plugin.h>
#include <selecta/viz.h>

#include "core/krk_lava_fx.h"

const sel_tag* sel_plugin_entry(uint32_t host_api);

static int fails;
#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            fprintf(stderr, "FAIL line %d: ", __LINE__); \
            fprintf(stderr, __VA_ARGS__);              \
            fprintf(stderr, "\n");                     \
            fails++;                                   \
        }                                              \
    } while (0)

static sel_viz_frame fr;
static uint32_t px[320 * 256];
static sel_surface surf;
static sel_viz_render_fn render;

static void frame(uint32_t n) {
    uint32_t i, b;
    for (i = 0; i < n; i++) {
        fr.seq++;
        fr.rate = 44100u;
        for (b = 0; b < SEL_VIZ_BANDS; b++) {
            fr.band[b] = (int16_t)(((fr.seq + b * 3u) % 12u) * 2000);
        }
        for (b = 0; b < SEL_VIZ_PCM_FRAMES * 2u; b++) {
            fr.pcm[b] = (int16_t)(((fr.seq * 7u + b) % 64u) * 400 - 12800);
        }
        CHECK(render(&fr, &surf, NULL) == 1u, "render drew nothing");
    }
}

static uint32_t distinct(void) {
    uint32_t seen[64], n = 0, i, j;
    for (i = 0; i < 320u * 256u && n < 64u; i += 37u) {
        for (j = 0; j < n && seen[j] != px[i]; j++) {
        }
        if (j == n) {
            seen[n++] = px[i];
        }
    }
    return n;
}

/* I1260 stage timers: a fake clock of 1 MHz that advances 50 ticks a read, so every lap is 50 us. */
extern uint32_t (*krk_stage_clock)(uint32_t* hz);
static uint32_t fake_now, fake_reads;
static uint32_t fake_clock(uint32_t* hz) {
    if (hz != NULL) {
        *hz = 1000000u;
    }
    fake_reads++;
    fake_now += 50u;
    return fake_now;
}

int main(void) {
    const sel_tag* desc = sel_plugin_entry(1u);
    const sel_tag* hooks = desc != NULL ? (const sel_tag*)sel_tag_find(desc, SEL_PLUGIN_TAGS, 0) : NULL;
    sel_viz_stats_fn stats = hooks != NULL ? (sel_viz_stats_fn)sel_tag_find(hooks, SEL_VIZ_STATS, 0) : NULL;
    sel_viz_key_fn key = hooks != NULL ? (sel_viz_key_fn)sel_tag_find(hooks, SEL_VIZ_KEY, 0) : NULL;
    sel_viz_indexed_fn indexed = hooks != NULL ? (sel_viz_indexed_fn)sel_tag_find(hooks, SEL_VIZ_INDEXED, 0) : NULL;
    char line[400];
    uint32_t before[320 * 256], i, diff = 0;
    uint32_t want[256];
    krk_params p;
    render = hooks != NULL ? (sel_viz_render_fn)sel_tag_find(hooks, SEL_VIZ_FRAME, 0) : NULL;
    if (render == NULL || stats == NULL || key == NULL) {
        fprintf(stderr, "plugin hooks missing\n");
        return 1;
    }
    surf.w = 320u;
    surf.h = 256u;
    surf.pitch = 320u * 4u;
    surf.fmt = SEL_PIX_RGBA32;
    surf.pixels = px;
    krk_stage_clock = fake_clock;
    frame(60);
    (void)stats(line, sizeof line);
    /* every stage is one lap of the fake clock: 50 us each, 12 stages */
    CHECK(fake_reads > 60u * 12u, "stage clock not read (%u)", (unsigned)fake_reads);
    CHECK(strstr(line, " stage_us forces 50 project 50 advect 50 particles 50 density 50 blur 50 light 50 bloom 50 "
                       "composite 50 inputs 50 palette 50 output 50 total 600") != NULL,
          "stage timers wrong: %s", line);
    CHECK(strstr(line, "gpu 2 ") != NULL, "fixed-point tier did not run: %s", line);
    CHECK(strstr(line, "steps 60 ") != NULL, "sim steps not counted: %s", line);
    CHECK(distinct() > 8u, "picture is flat (%u colours)", (unsigned)distinct());
    /* palette crossfade: select Magma, step half a fade, the ramp (so the pixels) moved; the sim index image is the same
     * kind of data but we compare ramps through two frames with the paused-free sim, so compare colours of identical
     * indices: pixel 0 is background, which blends between the palettes' bg stops. */
    memcpy(before, px, sizeof before);
    CHECK(key('3') == 1, "key 3 not used");
    frame(25); /* 1 s at 25 fps: inside the 3 s default fade */
    for (i = 0; i < 320u * 256u; i += 41u) {
        diff += before[i] != px[i];
    }
    CHECK(diff > 1000u, "palette change did not reach the pixels (%u)", (unsigned)diff);
    frame(400); /* well past fade plus lag: settled */
    krk_params_default(&p);
    krk_params_set(&p, KRK_K_PALETTE, 2.0f);
    krk_fx_palette(KRK_FX_LAVA, &p, want);
    (void)stats(line, sizeof line);
    CHECK(strstr(line, "palette 2 ") != NULL, "palette target not 2: %s", line);
    /* I1260 VIZ_INDEXED: the same frame as the plugin's own 160x128 index image and its 256 colours, no surface */
    {
        sel_viz_indexed ix;
        uint32_t seen[256], kinds = 0, j, bad = 0;
        memset(&ix, 0, sizeof ix);
        memset(seen, 0, sizeof seen);
        CHECK(indexed != NULL && indexed(&fr, NULL) == 0, "VIZ_INDEXED hook missing or took a NULL out");
        (void)stats(line, sizeof line);
        CHECK(indexed != NULL && indexed(&fr, &ix) == 1, "VIZ_INDEXED gave no frame");
        CHECK(ix.w == 160u && ix.h == 128u && ix.idx != NULL && ix.rgb != NULL, "VIZ_INDEXED size %ux%u", (unsigned)ix.w,
              (unsigned)ix.h);
        for (i = 0; ix.idx != NULL && i < ix.w * ix.h; i++) {
            kinds += seen[ix.idx[i]]++ == 0u;
        }
        for (j = 0; ix.rgb != NULL && j < 256u; j++) {
            bad += ix.rgb[j] != want[j];
        }
        CHECK(kinds > 8u, "index image is flat (%u indices)", (unsigned)kinds);
        CHECK(bad == 0u, "VIZ_INDEXED ramp is not the settled Magma ramp (%u entries differ)", (unsigned)bad);
        {
            char after[512];
            (void)stats(after, sizeof after);
            CHECK(strcmp(line, after) != 0, "VIZ_INDEXED did not advance the plugin: %s", after);
        }
    }
    /* every drawn colour is a ramp colour of the settled palette */
    for (i = 0; i < 320u * 256u; i += 53u) {
        uint32_t j, ok = 0;
        for (j = 0; j < 256u && !ok; j++) {
            ok = (px[i] & 0xFFFFFFu) == want[j];
        }
        CHECK(ok, "pixel %u colour %06x not in the settled Magma ramp", (unsigned)i, (unsigned)(px[i] & 0xFFFFFFu));
        if (!ok) {
            break;
        }
    }
    if (fails) {
        return 1;
    }
    printf("krk_fx_plugin_ok\n");
    return 0;
}
