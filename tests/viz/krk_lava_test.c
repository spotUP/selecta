/* krk-lava (I1052b, KRK-P0-4/5): the fixed-point Lava / Smoke 2D core, design
 * thoughts/shared/research/2026-10-27-krakatoa-port-design.md sections 3.2, 3.3, 3.6, 3.7 and 5.
 * Every case is driven by the shared krk_core: krk_audio_step -> krk_fx_inputs (krk_brush_at, params) -> krk_fx_step ->
 * krk_fx_render, the same calls the Amiga plugin makes.
 * 1. Output is 160x128 palette indices, deterministic: two runs give the same hash, and the image is not flat.
 * 2. Sentinels: frames == N, jacobi_iters == 8 per frame, particles were plotted (the new code ran).
 * 3. Float reference: a double-precision transliteration of the grid steps (same noise table, no quantisation) matches
 *    the fixed-point smoke / heat / velocity fields within the stated mean-error tolerance after 60 frames.
 * 4. A synthetic kick raises the heat injection on its own frame (ratio vs a no-kick control run, stated below).
 * 5. 10000 frames of changing music: velocity stays inside +-4 cells, fields stay in range, particles stay on screen,
 *    the image never goes flat.
 * 6. Knob coverage: every knob moves the Lava output (hash) except the stated exemptions; the palette knob moves the
 *    baked ramp.
 * 7. Smoke 2D: differs from Lava, plots no particles, still reacts to the music. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/krk_lava_fx.h"

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

#define DT (1.0f / 30.0f)
#define W ((int)KRK_FX_W)
#define H ((int)KRK_FX_H)
#define N ((int)KRK_FX_CELLS)

static sel_viz_frame fr;

static void frame_mix(uint32_t seq, int16_t bass, int16_t mid, int16_t hi) {
    uint32_t i;
    memset(&fr, 0, sizeof fr);
    fr.seq = seq;
    fr.rate = 48000u;
    fr.spectrum_valid = 1u;
    for (i = 0; i < 4u; i++) {
        fr.band[i] = bass;
    }
    for (i = 8; i < 12u; i++) {
        fr.band[i] = mid;
    }
    for (i = 12; i < 16u; i++) {
        fr.band[i] = hi;
    }
    for (i = 0; i < SEL_VIZ_PCM_FRAMES * 2u; i++) {
        fr.pcm[i] = (int16_t)((i / 64u) % 2u ? bass : -bass);
    }
}

/* The synthetic song: a bed with a kick every 30 frames and a snare half a beat later, highs wobbling. */
static void song(uint32_t f, int16_t* bass, int16_t* mid, int16_t* hi) {
    *bass = (f % 30u) == 0u ? 30000 : 4000;
    *mid = (f % 30u) == 15u ? 20000 : 2000;
    *hi = (int16_t)(2000 + (int)(f % 7u) * 400);
}

typedef struct run {
    krk_params p;
    krk_audio a;
    krk_fx* s;
    krk_fx_in in;
    uint32_t f;
    krk_fx_mode mode;
    uint8_t out[KRK_FX_OW * KRK_FX_OH];
} run;

static void run_init(run* r, krk_fx_mode mode) {
    memset(r, 0, sizeof *r);
    r->s = (krk_fx*)malloc(sizeof(krk_fx));
    krk_params_default(&r->p);
    krk_audio_init(&r->a);
    krk_fx_init(r->s);
    r->mode = mode;
}

static void run_free(run* r) { free(r->s); }

/* One frame of the whole chain; bass < 0 uses the song. */
static void run_frame(run* r, int render) {
    int16_t b, m, h;
    song(r->f + 1u, &b, &m, &h);
    frame_mix(r->f + 1u, b, m, h);
    krk_audio_step(&r->a, &fr, &r->p, DT);
    krk_fx_inputs(&r->in, r->mode, &r->p, &r->a, (float)r->f * DT, DT, r->f);
    krk_fx_step(r->s, &r->in);
    if (render) {
        krk_fx_render(r->s, &r->in, r->out);
    }
    r->f++;
}

static uint32_t hash_out(const uint8_t* o) {
    uint32_t h = 2166136261u, i;
    for (i = 0; i < KRK_FX_OW * KRK_FX_OH; i++) {
        h = (h ^ o[i]) * 16777619u;
    }
    return h;
}

static uint32_t run_hash(const krk_params* p, krk_fx_mode mode, uint32_t frames) {
    run* r = (run*)malloc(sizeof(run));
    uint32_t h, i;
    run_init(r, mode);
    r->p = *p;
    for (i = 0; i < frames; i++) {
        run_frame(r, i + 1u == frames);
    }
    h = hash_out(r->out);
    run_free(r);
    free(r);
    return h;
}

/* ---- float reference of the grid steps (case 3) ---- */
typedef struct ref {
    double u[N], v[N], p[N], dv[N], cu[N], heat[N], smoke[N], t0[N], t1[N];
} ref;

static int rcl(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double dcl(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int rcell(int x, int y) { return rcl(y, 0, H - 1) * W + rcl(x, 0, W - 1); }

static double rbil(const double* f, double fx, double fy) {
    int ix, iy, ix1, iy1;
    double ax, ay;
    fx = dcl(fx, 0, (W - 1) * 256.0);
    fy = dcl(fy, 0, (H - 1) * 256.0);
    ix = (int)(fx / 256.0);
    iy = (int)(fy / 256.0);
    ax = fx / 256.0 - ix;
    ay = fy / 256.0 - iy;
    ix1 = ix + 1 < W ? ix + 1 : ix;
    iy1 = iy + 1 < H ? iy + 1 : iy;
    return (f[iy * W + ix] * (1 - ax) + f[iy * W + ix1] * ax) * (1 - ay) +
           (f[iy1 * W + ix] * (1 - ax) + f[iy1 * W + ix1] * ax) * ay;
}

static void radd(ref* r, int i, double du, double dv) {
    r->u[i] = dcl(r->u[i] + du, -1024, 1024);
    r->v[i] = dcl(r->v[i] + dv, -1024, 1024);
}

static void ref_step(ref* r, const krk_fx_in* in, const krk_fx* s) {
    int x, y, k, nz = (int)KRK_FX_NOISE;
    if (in->brush_on) {
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                double dx = x * 256.0 - in->bx, dy = y * 256.0 - in->by, d2 = dx * dx + dy * dy, w;
                double r2 = (double)in->brad * in->brad;
                int i = y * W + x;
                if (d2 >= r2) {
                    continue;
                }
                w = 256.0 - d2 * 256.0 / r2;
                radd(r, i, (in->bvx - r->u[i]) * w / 1024.0 + dx * in->push / 256.0 * w / 4096.0,
                     (in->bvy - r->v[i]) * w / 1024.0 + dy * in->push / 256.0 * w / 4096.0);
                r->smoke[i] = dcl(r->smoke[i] + in->emis * w / 256.0, 0, 65535);
                r->heat[i] = dcl(r->heat[i] + in->heat * w / 256.0, 0, 65535);
            }
        }
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int i = y * W + x, ni = ((y + (in->noise_t >> 2)) & (nz - 1)) * nz + ((x + (in->noise_t >> 1)) & (nz - 1));
            double gate = (r->smoke[i] + r->heat[i]) / 512.0, buoy = r->heat[i] * in->lift / 65536.0;
            radd(r, i, s->noise[0][ni] * (double)in->turb / 128.0 * gate / 256.0,
                 buoy + s->noise[1][ni] * (double)in->turb / 128.0 * gate / 256.0);
        }
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            r->cu[y * W + x] = (r->v[rcell(x + 1, y)] - r->v[rcell(x - 1, y)]) - (r->u[rcell(x, y + 1)] - r->u[rcell(x, y - 1)]);
        }
    }
    for (y = 1; y < H - 1; y++) {
        for (x = 1; x < W - 1; x++) {
            int i = y * W + x;
            double gx = fabs(r->cu[i + 1]) - fabs(r->cu[i - 1]), gy = fabs(r->cu[i + W]) - fabs(r->cu[i - W]);
            double nrm = fabs(gx) + fabs(gy) + 1, nx = gx * 256 / nrm, ny = gy * 256 / nrm;
            radd(r, i, ny * r->cu[i] / 256.0 * in->conf / 4096.0, -(nx * r->cu[i] / 256.0 * in->conf / 4096.0));
        }
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            r->dv[y * W + x] = ((r->u[rcell(x + 1, y)] - r->u[rcell(x - 1, y)]) + (r->v[rcell(x, y + 1)] - r->v[rcell(x, y - 1)])) * 8.0;
            r->p[y * W + x] *= 13.0 / 16.0;
        }
    }
    for (k = 0; k < in->jacobi; k++) {
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                r->t0[y * W + x] = (r->p[rcell(x - 1, y)] + r->p[rcell(x + 1, y)] + r->p[rcell(x, y - 1)] +
                                    r->p[rcell(x, y + 1)] - r->dv[y * W + x]) / 4.0;
            }
        }
        memcpy(r->p, r->t0, sizeof r->p);
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            radd(r, y * W + x, -(r->p[rcell(x + 1, y)] - r->p[rcell(x - 1, y)]) / 32.0,
                 -(r->p[rcell(x, y + 1)] - r->p[rcell(x, y - 1)]) / 32.0);
        }
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int i = y * W + x;
            r->t0[i] = dcl(rbil(r->u, x * 256.0 - r->u[i], y * 256.0 - r->v[i]) * in->dec_vel / 65536.0, -1024, 1024);
            r->t1[i] = dcl(rbil(r->v, x * 256.0 - r->u[i], y * 256.0 - r->v[i]) * in->dec_vel / 65536.0, -1024, 1024);
        }
    }
    memcpy(r->u, r->t0, sizeof r->u);
    memcpy(r->v, r->t1, sizeof r->v);
    for (y = 0; y < H; y++) {
        r->u[y * W] = 0;
        r->u[y * W + W - 1] = 0;
    }
    for (x = 0; x < W; x++) {
        r->v[x] = 0;
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int i = y * W + x;
            r->t0[i] = rbil(r->heat, x * 256.0 - r->u[i], y * 256.0 - r->v[i]) * in->dec_heat / 65536.0;
            r->t1[i] = rbil(r->smoke, x * 256.0 - r->u[i], y * 256.0 - r->v[i]) * in->dec_smoke / 65536.0;
        }
    }
    memcpy(r->heat, r->t0, sizeof r->heat);
    memcpy(r->smoke, r->t1, sizeof r->smoke);
}

/* Pearson correlation of the fixed-point field against the reference. */
static double corr_u16(const uint16_t* a, const double* b) {
    double ma = 0, mb = 0, saa = 0, sbb = 0, sab = 0;
    int i;
    for (i = 0; i < N; i++) {
        ma += a[i];
        mb += b[i];
    }
    ma /= N;
    mb /= N;
    for (i = 0; i < N; i++) {
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
        sab += (a[i] - ma) * (b[i] - mb);
    }
    return sab / sqrt(saa * sbb + 1e-9);
}

/* The flow is chaotic (vorticity confinement), so quantisation noise grows with time. Stated tolerances, measured
 * 2026-11-10 (errors in the log line): after 20 frames the mean smoke error stays under 1.5% of full scale, heat under
 * 1.5%, velocity under 0.1 cells per frame; after 60 frames the smoke / heat fields still correlate above 0.9. */
static void test_float_reference(void) {
    run* a = (run*)malloc(sizeof(run));
    ref* r = (ref*)calloc(1, sizeof(ref));
    int i, f;
    double es = 0, eh = 0, eu = 0, ms = 0, c_s, c_h;
    run_init(a, KRK_FX_LAVA);
    for (f = 0; f < 60; f++) {
        run_frame(a, 0);
        ref_step(r, &a->in, a->s);
        if (f == 19) {
            for (i = 0; i < N; i++) {
                es += fabs(a->s->smoke[i] - r->smoke[i]);
                eh += fabs(a->s->heat[i] - r->heat[i]);
                eu += fabs(a->s->u[i] - r->u[i]) + fabs(a->s->v[i] - r->v[i]);
                ms += r->smoke[i];
            }
            es /= N * 65535.0;
            eh /= N * 65535.0;
            eu /= N * 256.0;
            printf("float reference after 20 frames: mean |smoke| err %.4f of full scale (ref mean %.4f), |heat| err %.4f, |vel| err %.3f cells/frame\n",
                   es, ms / N / 65535.0, eh, eu);
            CHECK(ms / N / 65535.0 > 0.01, "reference smoke is empty: the comparison proves nothing");
            CHECK(es < 0.015, "smoke error %.4f >= 0.015 of full scale", es);
            CHECK(eh < 0.015, "heat error %.4f >= 0.015 of full scale", eh);
            CHECK(eu < 0.1, "velocity error %.3f >= 0.1 cells per frame", eu);
        }
    }
    c_s = corr_u16(a->s->smoke, r->smoke);
    c_h = corr_u16(a->s->heat, r->heat);
    printf("float reference after 60 frames: smoke correlation %.4f, heat correlation %.4f\n", c_s, c_h);
    CHECK(c_s > 0.9, "smoke correlation %.4f <= 0.9", c_s);
    CHECK(c_h > 0.9, "heat correlation %.4f <= 0.9", c_h);
    run_free(a);
    free(a);
    free(r);
}

static void test_basic(void) {
    run* a = (run*)malloc(sizeof(run));
    uint32_t h1, h2, i, distinct = 0, seen[256];
    krk_params p;
    krk_params_default(&p);
    h1 = run_hash(&p, KRK_FX_LAVA, 120);
    h2 = run_hash(&p, KRK_FX_LAVA, 120);
    CHECK(h1 == h2, "two runs differ: %08x vs %08x", h1, h2);
    run_init(a, KRK_FX_LAVA);
    for (i = 0; i < 120; i++) {
        run_frame(a, i == 119);
    }
    memset(seen, 0, sizeof seen);
    for (i = 0; i < KRK_FX_OW * KRK_FX_OH; i++) {
        seen[a->out[i]] = 1;
    }
    for (i = 0; i < 256; i++) {
        distinct += seen[i];
    }
    printf("120 frames: hash %08x, %u distinct palette indices, frames %u, jacobi %u, plotted %u\n", h1, distinct,
           a->s->frames, a->s->jacobi_iters, a->s->particles_plotted);
    CHECK(distinct > 40u, "image nearly flat: %u distinct indices", distinct);
    CHECK(a->s->frames == 120u, "frames sentinel %u", a->s->frames);
    CHECK(a->s->jacobi_iters == 120u * 8u, "jacobi sentinel %u, want %u", a->s->jacobi_iters, 120u * 8u);
    CHECK(a->s->particles_plotted > 100000u, "particles plotted %u", a->s->particles_plotted);
    CHECK(KRK_FX_OW == 160u && KRK_FX_OH == 128u, "output size");
    run_free(a);
    free(a);
}

/* Heat injected at the brush on the kick frame: the same run with and without the kick. */
static void test_kick(void) {
    run* a = (run*)malloc(sizeof(run));
    run* b = (run*)malloc(sizeof(run));
    int i;
    double ha, hb, sa, sb;
    run_init(a, KRK_FX_LAVA);
    run_init(b, KRK_FX_LAVA);
    for (i = 0; i < 59; i++) {
        run_frame(a, 0);
        run_frame(b, 0);
    }
    /* frame 60 (f == 59 -> seq 60) is the kick in song(); the control gets a quiet frame instead */
    run_frame(a, 0);
    {
        int16_t bs = 4000, md = 2000, hi = 2000;
        frame_mix(60u, bs, md, hi);
        krk_audio_step(&b->a, &fr, &b->p, DT);
        krk_fx_inputs(&b->in, b->mode, &b->p, &b->a, (float)b->f * DT, DT, b->f);
        krk_fx_step(b->s, &b->in);
        b->f++;
    }
    ha = hb = sa = sb = 0;
    for (i = 0; i < N; i++) {
        ha += a->s->heat[i];
        hb += b->s->heat[i];
        sa += a->s->smoke[i];
        sb += b->s->smoke[i];
    }
    printf("kick frame: heat sum %.0f vs control %.0f (x%.2f), smoke sum %.0f vs %.0f (x%.2f), push %d vs %d\n", ha, hb,
           ha / hb, sa, sb, sa / sb, a->in.push, b->in.push);
    CHECK(ha > hb * 1.15, "kick heat x%.2f, want > 1.15", ha / hb);
    CHECK(a->in.push > b->in.push, "kick push %d not above control %d", a->in.push, b->in.push);
    run_free(a);
    run_free(b);
    free(a);
    free(b);
}

static void test_long(void) {
    run* a = (run*)malloc(sizeof(run));
    uint32_t i, flat = 0;
    int j, vmax = 0, bad = 0;
    run_init(a, KRK_FX_LAVA);
    for (i = 0; i < 10000u; i++) {
        int render = (i % 50u) == 0u;
        run_frame(a, render);
        for (j = 0; j < N; j++) {
            int au = abs(a->s->u[j]), av = abs(a->s->v[j]);
            if (au > vmax) {
                vmax = au;
            }
            if (av > vmax) {
                vmax = av;
            }
        }
        if (render) {
            uint32_t k, lo = 255, hi = 0;
            for (k = 0; k < KRK_FX_OW * KRK_FX_OH; k++) {
                if (a->out[k] < lo) {
                    lo = a->out[k];
                }
                if (a->out[k] > hi) {
                    hi = a->out[k];
                }
            }
            if (hi - lo < 40u) {
                flat++;
            }
        }
    }
    for (j = 0; j < (int)KRK_FX_MAXP; j++) {
        const krk_particle* q = &a->s->part[j];
        if (q->alive && (q->x < 0 || q->y < 0 || (q->x >> 16) >= (int32_t)KRK_FX_OW || (q->y >> 16) >= (int32_t)KRK_FX_OH)) {
            bad++;
        }
    }
    printf("10000 frames: max |velocity| %d (limit 1024), flat sampled frames %u of 200, off-screen alive particles %d\n",
           vmax, flat, bad);
    CHECK(vmax <= 1024, "velocity out of range: %d", vmax);
    CHECK(flat == 0u, "%u sampled frames went flat", flat);
    CHECK(bad == 0, "%d alive particles off screen", bad);
    CHECK(a->s->frames == 10000u, "frames %u", a->s->frames);
    run_free(a);
    free(a);
}

static void test_knobs(void) {
    krk_params base;
    uint32_t h0;
    int k;
    const krk_knob_info* t = krk_knob_table();
    krk_params_default(&base);
    h0 = run_hash(&base, KRK_FX_LAVA, 90);
    for (k = 0; k < (int)KRK_K_COUNT; k++) {
        krk_params p = base;
        float alt = base.v[k] == t[k].lo ? t[k].hi : t[k].lo;
        uint32_t h;
        if (k == (int)KRK_K_PAUSED) { /* exemption: the plugin skips krk_fx_step while paused (krakatoa.c), never reaches the core */
            continue;
        }
        if (k == (int)KRK_K_PALETTE) { /* the palette moves the baked ramp, not the indices */
            uint32_t r0[256], r1[256];
            krk_params_set(&p, (krk_knob)k, 2.0f);
            krk_fx_palette(KRK_FX_LAVA, &base, r0);
            krk_fx_palette(KRK_FX_LAVA, &p, r1);
            CHECK(memcmp(r0, r1, sizeof r0) != 0, "palette knob does not move the ramp");
            CHECK(r0[0] != r0[255] && r0[100] != r0[200], "ramp is flat");
            continue;
        }
        krk_params_set(&p, (krk_knob)k, alt);
        h = run_hash(&p, KRK_FX_LAVA, 90);
        if (h == h0) {
            CHECK(0, "knob %s (%g -> %g) does not move the Lava output", t[k].name, base.v[k], alt);
        }
    }
}

static void test_smoke2d(void) {
    run* a = (run*)malloc(sizeof(run));
    uint32_t i, hl, hs;
    krk_params p;
    krk_params_default(&p);
    hl = run_hash(&p, KRK_FX_LAVA, 90);
    hs = run_hash(&p, KRK_FX_SMOKE2D, 90);
    CHECK(hl != hs, "Smoke 2D equals Lava");
    run_init(a, KRK_FX_SMOKE2D);
    for (i = 0; i < 90u; i++) {
        run_frame(a, i == 89u);
    }
    CHECK(a->s->particles_plotted == 0u, "Smoke 2D plotted %u particles", a->s->particles_plotted);
    CHECK(a->s->jacobi_iters == 90u * 8u, "Smoke 2D jacobi %u", a->s->jacobi_iters);
    {
        uint32_t k, lo = 255, hi = 0;
        for (k = 0; k < KRK_FX_OW * KRK_FX_OH; k++) {
            if (a->out[k] < lo) {
                lo = a->out[k];
            }
            if (a->out[k] > hi) {
                hi = a->out[k];
            }
        }
        printf("Smoke 2D 90 frames: index range %u..%u, hash %08x vs Lava %08x\n", lo, hi, hs, hl);
        CHECK(hi - lo > 40u, "Smoke 2D image flat: %u..%u", lo, hi);
    }
    run_free(a);
    free(a);
}

int main(void) {
    test_basic();
    test_float_reference();
    test_kick();
    test_knobs();
    test_smoke2d();
    test_long();
    if (fails) {
        fprintf(stderr, "%d checks failed\n", fails);
        return 1;
    }
    printf("krk_lava_ok\n");
    return 0;
}
