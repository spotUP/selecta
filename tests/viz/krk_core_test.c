/* krk-core (I1052): the portable part of Krakatoa Smoke, design thoughts/shared/research/2026-10-27-krakatoa-port-design.md
 * section 2 (quality table, knobs, audio mapper). The GPU step and the plugin wiring are proven by dui-krakatoa.
 * 1. The four quality levels give the reference README grids at 1920x1080 and their Jacobi counts.
 * 2. Quality names parse; an unknown name falls back to high.
 * 3. Knobs clamp to their range.
 * 4. A bass spike over a steady bass bed is a beat; silence is none.
 * 5. Every knob moves the packed step (uniforms or brush). Exemption: paused, which the plugin reads to skip the step
 *    (krakatoa.c render), so it never reaches the uniforms; dui-krakatoa covers the stepping. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/krk_core.h"

static int fails;

#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            fprintf(stderr, "FAIL line %d: ", __LINE__);                                                               \
            fprintf(stderr, __VA_ARGS__);                                                                              \
            fprintf(stderr, "\n");                                                                                     \
            fails++;                                                                                                   \
        }                                                                                                              \
    } while (0)

static sel_viz_frame fr;

static void frame_bass(uint32_t seq, int16_t bass) {
    uint32_t i;
    memset(&fr, 0, sizeof fr);
    fr.seq = seq;
    fr.rate = 48000u;
    fr.spectrum_valid = 1u;
    for (i = 0; i < 4u; i++) {
        fr.band[i] = bass;
    }
    for (i = 0; i < SEL_VIZ_PCM_FRAMES * 2u; i++) {
        fr.pcm[i] = (int16_t)((i / 64u) % 2u ? bass : -bass);
    }
}

/* One packed step from a fixed music frame: what the plugin hands krk_gpu_step. */
static void pack(const krk_params* p, krk_uniforms* u, krk_brush* b) {
    krk_audio a;
    krk_grid g = krk_grid_for(KRK_Q_LOW, 1920u, 1080u);
    uint32_t i;
    krk_audio_init(&a);
    for (i = 1; i <= 30u; i++) {
        frame_bass(i, i == 30u ? 30000 : 4000);
        krk_audio_step(&a, &fr, p, 0.02f);
    }
    memset(u, 0, sizeof *u);
    memset(b, 0, sizeof *b);
    krk_brush_at(b, 0.6f, &g, p, &a);
    krk_uniforms_build(u, &g, p, &a, b, 0.02f, 0.6f, 960u, 540u, 7u);
}

int main(void) {
    static const uint32_t want[4][4] = {{100, 56, 36, 16}, {136, 76, 52, 20}, {200, 112, 76, 28}, {228, 128, 84, 30}};
    static const char* const names[4] = {"low", "medium", "high", "ultra"};
    const krk_knob_info* kt = krk_knob_table();
    krk_params p0, p;
    krk_uniforms u0, u;
    krk_brush b0, b;
    krk_audio a;
    uint32_t q, i, k;

    /* 1, 2 */
    for (q = 0; q < 4u; q++) {
        krk_grid g = krk_grid_for((krk_quality)q, 1920u, 1080u);
        CHECK(g.x == want[q][0] && g.y == want[q][1] && g.z == want[q][2], "%s grid %ux%ux%u, want %ux%ux%u", names[q],
              g.x, g.y, g.z, want[q][0], want[q][1], want[q][2]);
        CHECK(g.jacobi == want[q][3], "%s jacobi %u, want %u", names[q], g.jacobi, want[q][3]);
        CHECK(krk_quality_parse(names[q]) == (krk_quality)q, "parse %s", names[q]);
        CHECK(strcmp(krk_quality_name((krk_quality)q), names[q]) == 0, "name %u", q);
    }
    CHECK(krk_quality_parse("bogus") == KRK_Q_HIGH && krk_quality_parse(NULL) == KRK_Q_HIGH, "fallback to high");
    CHECK(krk_grid_for(KRK_Q_HIGH, 4000u, 100u).x == 336u, "a strip caps X at 3 Y");

    /* 3 */
    krk_params_default(&p);
    for (k = 0; k < KRK_K_COUNT; k++) {
        CHECK(p.v[k] == kt[k].def, "default %s", kt[k].name);
        krk_params_set(&p, (krk_knob)k, kt[k].hi + 100.0f);
        CHECK(p.v[k] == kt[k].hi, "%s clamps high", kt[k].name);
        krk_params_set(&p, (krk_knob)k, kt[k].lo - 100.0f);
        CHECK(p.v[k] == kt[k].lo, "%s clamps low", kt[k].name);
        CHECK(krk_knob_find(kt[k].name) == (int)k, "find %s", kt[k].name);
    }

    /* 4 */
    krk_params_default(&p);
    krk_audio_init(&a);
    for (i = 1; i <= 100u; i++) {
        frame_bass(i, 0);
        krk_audio_step(&a, &fr, &p, 0.02f);
    }
    CHECK(a.beats == 0u, "silence gave %u beats", a.beats);
    /* Steady bass for 4 s: the 1 s average settles near 4000, so the onset at the
       start stops firing after about 1.25 s and the spike lands far outside the
       200 ms refractory window (design section 4). */
    for (i = 101; i <= 300u; i++) {
        frame_bass(i, 4000);
        krk_audio_step(&a, &fr, &p, 0.02f);
    }
    k = a.beats;
    frame_bass(301u, 30000);
    krk_audio_step(&a, &fr, &p, 0.02f);
    CHECK(a.beats == k + 1u, "bass spike: beats %u -> %u", k, a.beats);
    CHECK(a.heat_mul > 1.0f && a.emission > 0.0f, "beat heat %f emission %f", (double)a.heat_mul, (double)a.emission);

    /* 5 */
    krk_params_default(&p0);
    pack(&p0, &u0, &b0);
    for (k = 0; k < KRK_K_COUNT; k++) {
        float v;
        if (k == KRK_K_PAUSED) {
            continue; /* exemption, see the header */
        }
        p = p0;
        v = fabsf(kt[k].hi - p0.v[k]) > fabsf(kt[k].lo - p0.v[k]) ? kt[k].hi : kt[k].lo;
        krk_params_set(&p, (krk_knob)k, v);
        pack(&p, &u, &b);
        CHECK(memcmp(&u, &u0, sizeof u) != 0 || memcmp(&b, &b0, sizeof b) != 0, "knob %s does not move the step",
              kt[k].name);
    }

    if (fails == 0) {
        printf("krk_core_ok\n");
    }
    return fails == 0 ? 0 : 1;
}
