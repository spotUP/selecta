/* Krakatoa Smoke visualizer (I559 / I1052, design thoughts/shared/research/2026-10-27-krakatoa-port-design.md, modern
 * tier: C host + compute shaders on the SDL3 GPU API). The music drives the brush: krk_audio_step turns each viz frame
 * into force, swirl, emission and beat heat; krk_brush_at moves the autopilot brush; krk_uniforms_build packs one step;
 * krk_gpu_step simulates and ray marches at half the surface size and returns a full-size image (its final pass
 * upscales, tone maps and dithers per surface pixel); this file copies it row by row into the surface.
 *
 * Quality comes from SELECTA_KRK_QUALITY (low, medium, high, ultra; default high) until the ui.krakatoa.quality pref
 * lands (KRK-P0-2). Knobs run at their design defaults until the same pref row wires them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <selecta/plugin.h>
#include <selecta/viz.h>

#include "core/krk_core.h"
#include "sdl/krk_smoke_gpu.h"

static struct {
    krk_gpu* gpu;
    int gpu_failed;
    krk_quality q;
    krk_grid g;
    krk_params p;
    krk_audio a;
    uint32_t sw, sh, ow, oh;
    uint32_t* img;
    uint32_t last_seq;
    float t;
    /* sentinels for --stats and the dui reachability test */
    uint32_t steps, jacobi_total;
    /* I1052d palette switching: the host services, the crossfade state and the sentinels */
    const sel_viz_host* host;
    krk_palfx fx;
    int fx_ready;
    uint32_t order_buf, order_ok, keys, palfx_steps;
} K;

static void krk_pref_save(void) {
    char v[8];
    if (K.host != NULL && K.host->pref_set != NULL) {
        (void)snprintf(v, sizeof v, "%u", (unsigned)K.fx.to);
        K.host->pref_set(K.host->user, KRK_PREF_PALETTE, v);
    }
}

static void viz_open(const sel_viz_host* host) {
    K.host = host;
}

/* First use, from a frame or a key (a key can arrive before the first frame): quality, params, prefs, palette state. */
static void krk_start(void) {
    if (K.q != KRK_Q_COUNT) {
        return;
    }
    K.q = krk_quality_parse(getenv("SELECTA_KRK_QUALITY"));
    krk_params_default(&K.p);
    krk_audio_init(&K.a);
    if (K.host != NULL && K.host->pref_get != NULL) {
        krk_params_parse(&K.p, K.host->pref_get(K.host->user, KRK_PREF_SMOKE, ""));
        krk_params_set(&K.p, KRK_K_AUTOPALETTE, (float)atof(K.host->pref_get(K.host->user, KRK_PREF_AUTOPALETTE, "1")));
    }
    krk_palfx_init(&K.fx, K.host != NULL && K.host->pref_get != NULL ? (uint32_t)atoi(K.host->pref_get(K.host->user, KRK_PREF_PALETTE, "0")) % KRK_PALETTES : 0u, &K.p);
    K.fx_ready = 1;
}

/* Keys 1-5 pick Aurora, Nebula, Magma, Spectrum, Ash; A toggles the auto cycle. Both persist as prefs. */
static int viz_key(uint32_t code) {
    krk_start();
    if (code >= '1' && code <= '5') {
        K.keys++;
        (void)krk_palfx_select(&K.fx, code - '1', &K.p);
        krk_pref_save();
        return 1;
    }
    if (code == 'a') {
        K.keys++;
        krk_params_set(&K.p, KRK_K_AUTOPALETTE, K.p.v[KRK_K_AUTOPALETTE] < 0.5f ? 1.0f : 0.0f);
        if (K.host != NULL && K.host->pref_set != NULL) {
            K.host->pref_set(K.host->user, KRK_PREF_AUTOPALETTE, K.p.v[KRK_K_AUTOPALETTE] < 0.5f ? "0" : "1");
        }
        return 1;
    }
    return 0;
}

static void krk_reset(uint32_t sw, uint32_t sh) {
    krk_gpu_destroy(K.gpu);
    K.gpu = NULL;
    free(K.img);
    K.img = NULL;
    K.sw = sw;
    K.sh = sh;
    K.ow = sw / 2u;
    K.oh = sh / 2u;
    K.g = krk_grid_for(K.q, sw, sh);
    K.gpu_failed = 0;
    if (K.ow == 0u || K.oh == 0u) {
        K.gpu_failed = 1;
        return;
    }
    K.img = (uint32_t*)malloc((size_t)sw * sh * 4u);
    K.gpu = K.img != NULL ? krk_gpu_create(&K.g, sw, sh) : NULL;
    K.gpu_failed = K.gpu == NULL;
}

static uint32_t render(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c) {
    krk_uniforms u;
    krk_brush b;
    float dt;
    uint32_t d, y, n;
    (void)c;
    if (s == NULL || s->fmt != SEL_PIX_RGBA32 || s->w < 2u || s->h < 2u) {
        return 0;
    }
    krk_start();
    if (s->w != K.sw || s->h != K.sh) {
        krk_reset(s->w, s->h);
    }
    if (K.gpu_failed) {
        return 0;
    }
    if (K.p.v[KRK_K_PAUSED] < 0.5f) {
        d = f != NULL ? f->seq - K.last_seq : 0u;
        dt = d == 0u || d > 5u ? 1.0f / 60.0f : (float)d / (float)SEL_VIZ_BLOCKS_PER_SECOND;
        K.last_seq = f != NULL ? f->seq : 0u;
        K.t += dt;
        krk_audio_step(&K.a, f, &K.p, dt);
        K.order_ok = K.host != NULL && K.host->position != NULL && K.host->position(K.host->user, &K.order_buf);
        if (krk_palfx_step(&K.fx, &K.p, &K.a, K.order_ok ? &K.order_buf : NULL, dt)) {
            krk_pref_save();
        }
        K.palfx_steps++;
        krk_brush_at(&b, K.t, &K.g, &K.p, &K.a);
        krk_uniforms_build(&u, &K.g, &K.p, &K.a, &b, dt, K.t, K.ow, K.oh, K.steps);
        krk_uniforms_palette(&u, &K.fx);
        n = krk_gpu_step(K.gpu, &u, K.g.jacobi, K.img);
        if (n == 0u) {
            return 0;
        }
        K.steps++;
        K.jacobi_total += n;
    }
    for (y = 0; y < s->h; y++) {
        memcpy((uint8_t*)s->pixels + (size_t)y * s->pitch, K.img + (size_t)y * s->w, (size_t)s->w * 4u);
    }
    return 1;
}

static uint32_t stats(char* out, uint32_t cap) {
    int n;
    if (out == NULL || cap == 0u) {
        return 0;
    }
    n = snprintf(out, cap, "krakatoa grid %ux%ux%u jacobi %u steps %u jacobi_total %u beats %u surface %ux%u quality %s gpu %d palette %u changes %u cues %u keys %u palfx_steps %u",
                 (unsigned)K.g.x, (unsigned)K.g.y, (unsigned)K.g.z, (unsigned)K.g.jacobi, (unsigned)K.steps,
                 (unsigned)K.jacobi_total, (unsigned)K.a.beats, (unsigned)K.sw, (unsigned)K.sh,
                 krk_quality_name(K.q == KRK_Q_COUNT ? KRK_Q_DEFAULT : K.q), K.gpu != NULL ? 1 : 0, (unsigned)K.fx.to, (unsigned)K.fx.changes, (unsigned)K.fx.cues, (unsigned)K.keys,
                 (unsigned)K.palfx_steps);
    return n < 0 ? 0u : (uint32_t)n < cap ? (uint32_t)n : cap - 1u;
}

/* No VIZ_CLOSE hook: the host never calls one yet (KRK-P0-1 session hook); the device lives until exit. */
static sel_tag k_hooks[6];
static sel_tag k_desc[5];

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    K.q = KRK_Q_COUNT;
    k_hooks[0].tag = SEL_VIZ_NAME;
    k_hooks[0].value = (uintptr_t) "Krakatoa";
    k_hooks[1].tag = SEL_VIZ_FRAME;
    k_hooks[1].value = SEL_TAG_FN(render);
    k_hooks[2].tag = SEL_VIZ_STATS;
    k_hooks[2].value = SEL_TAG_FN(stats);
    k_hooks[3].tag = SEL_VIZ_OPEN;
    k_hooks[3].value = SEL_TAG_FN(viz_open);
    k_hooks[4].tag = SEL_VIZ_KEY;
    k_hooks[4].value = SEL_TAG_FN(viz_key);
    k_hooks[5].tag = SEL_TAG_END;
    k_hooks[5].value = 0;
    k_desc[0].tag = SEL_PLUGIN_KIND;
    k_desc[0].value = SEL_PLUGIN_KIND_VIZ;
    k_desc[1].tag = SEL_PLUGIN_ID;
    k_desc[1].value = (uintptr_t) "viz_krakatoa";
    k_desc[2].tag = SEL_PLUGIN_VERSION;
    k_desc[2].value = 1;
    k_desc[3].tag = SEL_PLUGIN_TAGS;
    k_desc[3].value = (uintptr_t)k_hooks;
    k_desc[4].tag = SEL_TAG_END;
    k_desc[4].value = 0;
    return k_desc;
}
