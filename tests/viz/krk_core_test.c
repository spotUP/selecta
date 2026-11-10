/* krk-core (I1052): the portable part of Krakatoa Smoke, design thoughts/shared/research/2026-10-27-krakatoa-port-design.md
 * section 2 (quality table, knobs, audio mapper). The GPU step and the plugin wiring are proven by dui-krakatoa.
 * 1. The four quality levels give the reference README grids at 1920x1080 and their Jacobi counts.
 * 2. Quality names parse; an unknown name falls back to high.
 * 3. Knobs clamp to their range.
 * 4. A bass spike over a steady bass bed is a beat; silence is none.
 * 5. Every knob moves the packed step (uniforms or brush). Exemptions: paused, which the plugin reads to skip the step
 *    (krakatoa.c render), so it never reaches the uniforms; dui-krakatoa covers the stepping. smoke, particles,
 *    showSmoke, showParticles are Lava-only (design 1.4); Lava is KRK-P0-4, so they move no Smoke output yet.
 * 6. I1246 kick: the push (uPush), emission and brush radius rise on the very frame of the kick (latency 0 frames),
 *    the pump then swings below rest (speaker cone relax); kickBurst 0 / kickPump 0 remove each reaction.
 * 7. I1246 snare: a mid-band hit flashes the light and bloom on its frame; a kick alone never flashes; snareFlash 0
 *    removes it. A snare is not counted as a kick.
 * 8. Band normalisation floor (I1052x): near-silent noise stays below 0.1 instead of being scaled to full.
 * 9. Pref string: ui.krakatoa.smoke "name=value,..." sets knobs, skips bad entries, clamps.
 * 10. Palettes: 5 named ramps (reference hex values), all with ember and tail, the five hot[0] colours distinct; the palette knob changes
 *    the packed hot ramp.
 * 11. I1052d palette transitions: krk_palette_blend gives exactly palette a at t=0, b at t=1 and the midpoint at 0.5;
 *    a selection crossfades over `fade` seconds with the fire ramp leading and the smoke body lagging; the auto cue
 *    fires on a tracker order change (not on rows within an order), at most once per `paletteInterval` seconds, and
 *    cycles all five palettes; without a play position an energy jump cues; autoPalette 0 never cues; fade 0 settles at
 *    once; each of the three knobs moves the packed colours (krk_uniforms_palette). */
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

/* A frame with bass (bands 0-3), mid (bands 8-11, the snare band) and highs (bands 12-15). */
static void frame_mix(uint32_t seq, int16_t bass, int16_t mid, int16_t hi) {
    uint32_t i;
    frame_bass(seq, bass);
    for (i = 8; i < 12u; i++) {
        fr.band[i] = mid;
    }
    for (i = 12; i < 16u; i++) {
        fr.band[i] = hi;
    }
}

/* One packed step from a fixed music frame: what the plugin hands krk_gpu_step. */
static void pack(const krk_params* p, krk_uniforms* u, krk_brush* b) {
    krk_audio a;
    krk_grid g = krk_grid_for(KRK_Q_LOW, 1920u, 1080u);
    uint32_t i;
    krk_audio_init(&a);
    for (i = 1; i <= 30u; i++) {
        frame_mix(i, i == 30u ? 30000 : 4000, i == 28u ? 20000 : 2000, 3000);
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
        if (k == KRK_K_PAUSED || k == KRK_K_SMOKE || k == KRK_K_PARTICLES || k == KRK_K_SHOWSMOKE ||
            k == KRK_K_SHOWPARTICLES || k == KRK_K_AUTOPALETTE || k == KRK_K_FADE || k == KRK_K_PALINTERVAL) {
            continue; /* exemptions, see the header; the three palette transition knobs are proven in 11 */
        }
        p = p0;
        v = fabsf(kt[k].hi - p0.v[k]) > fabsf(kt[k].lo - p0.v[k]) ? kt[k].hi : kt[k].lo;
        krk_params_set(&p, (krk_knob)k, v);
        pack(&p, &u, &b);
        CHECK(memcmp(&u, &u0, sizeof u) != 0 || memcmp(&b, &b0, sizeof b) != 0, "knob %s does not move the step",
              kt[k].name);
    }

    /* 6, 7: a groove (steady bass, mids, highs for 2 s), then one kick, later one snare. */
    {
        static const float knob_off[3] = {KRK_K_KICKBURST, KRK_K_KICKPUMP, KRK_K_SNAREFLASH};
        krk_grid g = krk_grid_for(KRK_Q_LOW, 1920u, 1080u);
        uint32_t run;
        for (run = 0; run < 4u; run++) {
            float push0, emis0, rad0, push_k = 0.0f, emis_k = 0.0f, rad_k = 0.0f, pump_min = 0.0f, flash_k = 0.0f;
            float light0, bloom0, light_s = 0.0f, bloom_s = 0.0f;
            uint32_t beats0, snares0, beats_s;
            krk_params_default(&p);
            if (run > 0u) {
                krk_params_set(&p, (krk_knob)(int)knob_off[run - 1u], 0.0f);
            }
            krk_audio_init(&a);
            for (i = 1; i <= 100u; i++) {
                frame_mix(i, 6000, 3000, 2000);
                krk_audio_step(&a, &fr, &p, 0.02f);
            }
            krk_brush_at(&b, 2.0f, &g, &p, &a);
            krk_uniforms_build(&u, &g, &p, &a, &b, 0.02f, 2.0f, 960u, 540u, 1u);
            push0 = u.forces[3];
            emis0 = u.rates[0];
            rad0 = b.radius;
            light0 = u.lightp[3];
            bloom0 = u.rend[3];
            beats0 = a.beats;
            snares0 = a.snares;
            /* the kick frame: response measured on this same step, frame 0 */
            frame_mix(101u, 26000, 3000, 2000);
            krk_audio_step(&a, &fr, &p, 0.02f);
            krk_brush_at(&b, 2.02f, &g, &p, &a);
            krk_uniforms_build(&u, &g, &p, &a, &b, 0.02f, 2.02f, 960u, 540u, 2u);
            push_k = u.forces[3];
            emis_k = u.rates[0];
            rad_k = b.radius;
            flash_k = a.flash;
            for (i = 102; i <= 130u; i++) {
                frame_mix(i, 6000, 3000, 2000);
                krk_audio_step(&a, &fr, &p, 0.02f);
                if (a.pump < pump_min) {
                    pump_min = a.pump;
                }
            }
            beats_s = a.beats;
            /* the snare frame */
            frame_mix(131u, 6000, 24000, 2000);
            krk_audio_step(&a, &fr, &p, 0.02f);
            krk_brush_at(&b, 2.62f, &g, &p, &a);
            krk_uniforms_build(&u, &g, &p, &a, &b, 0.02f, 2.62f, 960u, 540u, 3u);
            light_s = u.lightp[3];
            bloom_s = u.rend[3];
            CHECK(a.beats == beats0 + 1u && beats_s == a.beats, "run %u: one kick, the snare is no kick (%u %u %u)", run,
                  beats0, beats_s, a.beats);
            CHECK(a.snares == snares0 + 1u, "run %u: one snare (%u -> %u)", run, snares0, a.snares);
            CHECK(flash_k == 0.0f, "run %u: a kick alone flashes %f", run, (double)flash_k);
            if (run != 2u) {
                CHECK(push_k > 2.0f * push0, "run %u: kick push %f over %f within 0 frames", run, (double)push_k,
                      (double)push0);
                CHECK(pump_min < 0.0f, "run %u: pump never relaxes below rest", run);
                CHECK(rad_k > rad0 * 1.2f, "run %u: emitter radius %f over %f", run, (double)rad_k, (double)rad0);
            } else {
                CHECK(push_k <= push0 * 1.01f && rad_k == rad0, "kickPump 0 still pumps: %f over %f", (double)push_k,
                      (double)push0);
            }
            if (run != 1u) {
                CHECK(emis_k > 2.0f * emis0, "run %u: kick emission %f over %f within 0 frames", run, (double)emis_k,
                      (double)emis0);
            }
            if (run == 1u) {
                CHECK(emis_k <= emis0 * 1.05f, "kickBurst 0 still bursts: %f over %f", (double)emis_k, (double)emis0);
            }
            if (run != 3u) {
                CHECK(light_s > 1.5f * light0 && bloom_s > 1.5f * bloom0, "run %u: snare light %f/%f bloom %f/%f", run,
                      (double)light_s, (double)light0, (double)bloom_s, (double)bloom0);
            } else {
                CHECK(light_s == light0 && bloom_s == bloom0, "snareFlash 0 still flashes");
            }
        }
    }
    /* 8 */
    krk_params_default(&p);
    krk_audio_init(&a);
    for (i = 1; i <= 100u; i++) {
        frame_mix(i, 40, 40, 40);
        krk_audio_step(&a, &fr, &p, 0.02f);
    }
    CHECK(a.B < 0.1f && a.M < 0.1f && a.H < 0.1f, "noise floor: B %f M %f H %f", (double)a.B, (double)a.M, (double)a.H);
    CHECK(a.beats == 0u && a.snares == 0u, "noise gave %u kicks %u snares", a.beats, a.snares);
    /* 9 */
    krk_params_default(&p);
    k = krk_params_parse(&p, "swirl=40,bogus=1,force=x,palette=2.4,bloom=9,kickPump=");
    CHECK(k == 3u, "parse set %u knobs, want 3", k);
    CHECK(p.v[KRK_K_SWIRL] == 40.0f && p.v[KRK_K_PALETTE] == 2.0f && p.v[KRK_K_BLOOM] == 2.0f &&
              p.v[KRK_K_FORCE] == kt[KRK_K_FORCE].def && p.v[KRK_K_KICKPUMP] == kt[KRK_K_KICKPUMP].def,
          "parse values");
    CHECK(krk_params_parse(&p, NULL) == 0u && krk_params_parse(&p, "") == 0u, "empty parse");
    /* 10 */
    {
        uint32_t j;
        for (i = 0; i < KRK_PALETTES; i++) {
            const krk_palette* pa = krk_palette_get(i);
            CHECK(pa->name != NULL && krk_rgb_hex(pa->ember) != 0u && krk_rgb_hex(pa->tail) != 0u, "palette %u fields", i);
            for (j = 0; j < i; j++) {
                CHECK(krk_rgb_hex(pa->hot[0]) != krk_rgb_hex(krk_palette_get(j)->hot[0]), "palette %u hot0 = %u", i, j);
            }
        }
        CHECK(strcmp(krk_palette_get(2u)->name, "Magma") == 0 && krk_palette_get(99u) == krk_palette_get(0u),
              "palette order and fallback");
        /* Reference index.html PALETTES (lines 275-281) through its toLin, (c/255)^2.2: Aurora hot[0] #d6ff3c,
         * Ash smoke #a4acbc, Magma bg[1] #1a0605. */
        {
            const float* got[3] = {krk_palette_get(0u)->hot[0], krk_palette_get(4u)->smoke, krk_palette_get(2u)->bg[1]};
            const uint32_t ref_hex[3] = {0xd6ff3cu, 0xa4acbcu, 0x1a0605u};
            for (uint32_t ri = 0; ri < 3u; ri++)
                for (uint32_t ci = 0; ci < 3u; ci++) {
                    const float lin = powf((float)((ref_hex[ri] >> (16u - 8u * ci)) & 255u) / 255.0f, 2.2f);
                    CHECK(fabsf(got[ri][ci] - lin) < 1e-3f, "reference colour %u channel %u: %f, want %f", ri, ci,
                          (double)got[ri][ci], (double)lin);
                }
        }
        CHECK(krk_rgb_hex((const float[3]){1.0f, 0.0f, 0.5f}) == 0xFF00BCu, "sRGB hex %06X",
              krk_rgb_hex((const float[3]){1.0f, 0.0f, 0.5f}));
    }

    /* 11 */
    {
        krk_palette bl, fire, body;
        krk_palfx fx;
        krk_uniforms ua, ub;
        const krk_palette *pa = krk_palette_get(0u), *pb = krk_palette_get(2u);
        uint32_t ord, seen[KRK_PALETTES] = {0}, n, step, cues0;
        float tt;
        krk_params_default(&p);
        krk_audio_init(&a);
        krk_palette_blend(&bl, pa, pb, 0.0f);
        CHECK(memcmp(bl.hot, pa->hot, sizeof bl.hot) == 0 && memcmp(bl.smoke, pa->smoke, sizeof bl.smoke) == 0, "blend t=0");
        krk_palette_blend(&bl, pa, pb, 1.0f);
        CHECK(memcmp(bl.hot, pb->hot, sizeof bl.hot) == 0 && memcmp(bl.bg, pb->bg, sizeof bl.bg) == 0 &&
                  memcmp(bl.tail, pb->tail, sizeof bl.tail) == 0, "blend t=1");
        krk_palette_blend(&bl, pa, pb, 0.5f);
        for (i = 0; i < 3u; i++) {
            CHECK(bl.hot[1][i] == (pa->hot[1][i] + pb->hot[1][i]) * 0.5f && bl.smoke[i] == (pa->smoke[i] + pb->smoke[i]) * 0.5f,
                  "blend t=0.5 channel %u", i);
        }
        /* a selection: fire leads, body lags, both exact at the ends */
        krk_palfx_init(&fx, 0u, &p);
        CHECK(krk_palfx_select(&fx, 2u, &p) == 1 && krk_palfx_select(&fx, 2u, &p) == 0, "select starts once");
        krk_palfx_colors(&fx, &fire, &body);
        CHECK(memcmp(fire.hot, pa->hot, sizeof fire.hot) == 0 && memcmp(body.smoke, pa->smoke, sizeof body.smoke) == 0, "t=0 old");
        for (i = 0; i < 75u; i++) { /* 1.5 s of the 3 s fade */
            krk_palfx_step(&fx, &p, &a, NULL, 0.02f);
        }
        krk_palfx_colors(&fx, &fire, &body);
        tt = fabsf(fire.hot[1][0] - pa->hot[1][0]) / fabsf(pb->hot[1][0] - pa->hot[1][0]);
        CHECK(fabsf(fx.t - 0.5f) < 0.02f && tt > 0.6f, "fire leads at half time: moved %f", (double)tt);
        tt = fabsf(body.smoke[0] - pa->smoke[0]) / fabsf(pb->smoke[0] - pa->smoke[0]);
        CHECK(tt < 0.4f, "body lags at half time: moved %f", (double)tt);
        for (i = 0; i < 80u; i++) {
            krk_palfx_step(&fx, &p, &a, NULL, 0.02f);
        }
        krk_palfx_colors(&fx, &fire, &body);
        CHECK(fx.t == 1.0f && memcmp(fire.hot, pb->hot, sizeof fire.hot) == 0 && memcmp(body.smoke, pb->smoke, sizeof body.smoke) == 0,
              "t=1 new");
        /* auto cue: order changes at 10 s intervals cue at most every 30 s, all five palettes appear */
        krk_palfx_init(&fx, 0u, &p);
        ord = 0u;
        seen[0] = 1u;
        for (step = 1; step <= 6000u; step++) { /* 120 s at 50 Hz */
            if (step % 500u == 0u) {
                ord++; /* pattern boundary every 10 s */
            }
            n = fx.cues;
            krk_palfx_step(&fx, &p, &a, &ord, 0.02f);
            if (fx.cues != n) {
                CHECK(step == 500u || step % 1500u == 500u, "cue at step %u (want 10 s, then every 30 s)", step);
            }
            seen[fx.to] = 1u;
        }
        CHECK(fx.cues == 4u, "120 s with a boundary every 10 s: %u cues, want 4", fx.cues);
        for (i = 0; i < KRK_PALETTES; i++) {
            CHECK(seen[i] == 1u, "palette %u never shown by the cycle", i);
        }
        /* rows moving inside one order is no boundary */
        krk_palfx_init(&fx, 0u, &p);
        ord = 3u;
        for (step = 0; step < 3000u; step++) {
            krk_palfx_step(&fx, &p, &a, &ord, 0.02f);
        }
        CHECK(fx.cues == 0u, "no order change, no cue");
        /* knob autoPalette */
        krk_params_set(&p, KRK_K_AUTOPALETTE, 0.0f);
        krk_palfx_init(&fx, 0u, &p);
        for (step = 0; step < 3000u; step++) {
            ord = step / 100u;
            krk_palfx_step(&fx, &p, &a, &ord, 0.02f);
        }
        CHECK(fx.cues == 0u && fx.to == 0u, "autoPalette 0 never cues");
        /* knob paletteInterval: 5 s allows far more cues than 30 s */
        krk_params_default(&p);
        krk_palfx_init(&fx, 0u, &p);
        for (step = 0; step < 6000u; step++) {
            ord = step / 100u; /* a boundary every 2 s */
            krk_palfx_step(&fx, &p, &a, &ord, 0.02f);
        }
        cues0 = fx.cues;
        krk_params_set(&p, KRK_K_PALINTERVAL, 5.0f);
        krk_palfx_init(&fx, 0u, &p);
        for (step = 0; step < 6000u; step++) {
            ord = step / 100u;
            krk_palfx_step(&fx, &p, &a, &ord, 0.02f);
        }
        CHECK(fx.cues > cues0 * 3u, "paletteInterval 5 s: %u cues vs %u at 30 s", fx.cues, cues0);
        /* knob fade: the colour after 1 s differs between fade 3 and fade 1, and fade 0 is settled at once */
        {
            krk_params pf = p;
            krk_palfx f3, f1;
            krk_palette fa, fb, bd;
            krk_params_default(&pf);
            krk_palfx_init(&f3, 0u, &pf);
            krk_palfx_select(&f3, 2u, &pf);
            krk_params_set(&pf, KRK_K_FADE, 1.0f);
            krk_palfx_init(&f1, 0u, &pf);
            krk_palfx_select(&f1, 2u, &pf);
            for (i = 0; i < 55u; i++) {
                krk_palfx_step(&f3, &p, &a, NULL, 0.02f);
                krk_palfx_step(&f1, &pf, &a, NULL, 0.02f);
            }
            krk_palfx_colors(&f3, &fa, &bd);
            krk_palfx_colors(&f1, &fb, &bd);
            CHECK(fb.hot[1][0] != fa.hot[1][0] && f1.t >= 1.0f && f3.t < 0.5f, "fade seconds move the blend (t %f vs %f)",
                  (double)f3.t, (double)f1.t);
            krk_params_set(&pf, KRK_K_FADE, 0.0f);
            krk_palfx_init(&f1, 0u, &pf);
            krk_palfx_select(&f1, 2u, &pf);
            CHECK(f1.t == 1.0f && f1.from == 2u, "fade 0 settles at once");
        }
        /* energy jump cue for a format with no position */
        krk_params_default(&p);
        krk_palfx_init(&fx, 0u, &p);
        a.env = 0.2f;
        for (step = 0; step < 500u; step++) {
            krk_palfx_step(&fx, &p, &a, NULL, 0.02f);
        }
        CHECK(fx.cues == 0u, "steady energy, no cue");
        a.env = 0.9f;
        krk_palfx_step(&fx, &p, &a, NULL, 0.02f);
        CHECK(fx.cues == 1u && fx.to == 1u, "energy jump cues the next palette");
        /* the packed colours: the fade, autoPalette-independent, reach the uniforms */
        krk_params_default(&p);
        pack(&p, &ua, &b0);
        krk_palfx_init(&fx, 0u, &p);
        krk_palfx_select(&fx, 3u, &p);
        krk_uniforms_palette(&ua, &fx);
        ub = ua;
        for (i = 0; i < 160u; i++) {
            krk_palfx_step(&fx, &p, &a, NULL, 0.02f);
        }
        krk_uniforms_palette(&ub, &fx);
        CHECK(memcmp(ua.hot, ub.hot, sizeof ua.hot) != 0 && memcmp(ua.smoke, ub.smoke, sizeof ua.smoke) != 0,
              "the crossfade moves the packed palette");
        CHECK(ub.hot[0][0] == krk_palette_get(3u)->hot[0][0] && ub.bg[1][1] == krk_palette_get(3u)->bg[1][1], "settled uniforms");
    }
    if (fails == 0) {
        printf("krk_core_ok\n");
    }
    return fails == 0 ? 0 : 1;
}
