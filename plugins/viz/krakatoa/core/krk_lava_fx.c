/* Krakatoa fixed-point Lava core, see krk_lava_fx.h. Integer only inside krk_fx_step / krk_fx_render. */
#include "krk_lava_fx.h"

#include <math.h>
#include <string.h>

#define W ((int32_t)KRK_FX_W)
#define H ((int32_t)KRK_FX_H)
#define DW ((int32_t)KRK_FX_DW)
#define DH ((int32_t)KRK_FX_DH)
#define OW ((int32_t)KRK_FX_OW)
#define OH ((int32_t)KRK_FX_OH)
#define NZ ((int32_t)KRK_FX_NOISE)
#define VMAX 1024 /* +-4 cells per frame in 8.8 (CFL for bilinear advection) */

static int32_t cl(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int32_t iabs(int32_t v) { return v < 0 ? -v : v; }

/* Bilinear sample at 8.8 cell coordinates, clamped to the grid. The position (cell offset, neighbour steps, weights)
 * is set up once and shared by every field sampled at that point. Each lerp is a*256 + (b - a)*w, which equals
 * a*(256 - w) + b*w with one multiply instead of two; two 8-bit weight steps keep it inside 32 bits. */
typedef struct bil_pos {
    int32_t o, dx, dy, ax, ay;
} bil_pos;

static inline void bil_setup(bil_pos* b, int32_t fx, int32_t fy) {
    int32_t ix, iy;
    fx = cl(fx, 0, (W - 1) << 8);
    fy = cl(fy, 0, (H - 1) << 8);
    ix = fx >> 8;
    iy = fy >> 8;
    b->ax = fx & 255;
    b->ay = fy & 255;
    b->o = iy * W + ix;
    b->dx = ix + 1 < W ? 1 : 0;
    b->dy = iy + 1 < H ? W : 0;
}

#define BILINEAR(NAME, T)                                                                                              \
    static inline int32_t NAME(const T* f, const bil_pos* b) {                                                         \
        const T* q = f + b->o;                                                                                         \
        int32_t a = q[0], top, bot;                                                                                    \
        top = (a * 256 + ((int32_t)q[b->dx] - a) * b->ax + 128) >> 8;                                                  \
        a = q[b->dy];                                                                                                  \
        bot = (a * 256 + ((int32_t)q[b->dy + b->dx] - a) * b->ax + 128) >> 8;                                          \
        return (top * 256 + (bot - top) * b->ay + 128) >> 8;                                                           \
    }
BILINEAR(bil_s, int16_t)
BILINEAR(bil_u, uint16_t)

/* One pass over the grid with a clamped 4-neighbour stencil. The body sees i (this cell), il / ir (left, right) and
 * i + om / i + op (row below, row above); the clamps are resolved per row and at the two row ends, so the 38 interior
 * cells of a row run with no bounds test. */
#define STENCIL(...)                                                                                                   \
    for (y = 0; y < H; y++) {                                                                                          \
        const int32_t row = y * W, om = y > 0 ? -W : 0, op = y < H - 1 ? W : 0;                                        \
        int32_t i = row, il = row, ir = row + 1;                                                                       \
        {__VA_ARGS__}                                                                                                  \
        for (i = row + 1; i < row + W - 1; i++) {                                                                      \
            il = i - 1;                                                                                                \
            ir = i + 1;                                                                                                \
            {__VA_ARGS__}                                                                                              \
        }                                                                                                              \
        il = i - 1;                                                                                                    \
        ir = i;                                                                                                        \
        {__VA_ARGS__}                                                                                                  \
    }

krk_grid krk_fx_grid(void) {
    krk_grid g;
    g.x = KRK_FX_W;
    g.y = KRK_FX_H;
    g.z = 1;
    g.jacobi = KRK_FX_JACOBI;
    return g;
}

/* ---- noise table: curl of a periodic smooth value-noise potential, integer only ---- */
static uint32_t hash2(uint32_t x, uint32_t y) {
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void noise_build(krk_fx* s) {
    static int16_t psi[KRK_FX_NOISE * KRK_FX_NOISE];
    int32_t x, y;
    for (y = 0; y < NZ; y++) {
        for (x = 0; x < NZ; x++) {
            int32_t lx = x >> 3, ly = y >> 3, fx = (x & 7) * 32, fy = (y & 7) * 32, sx, sy, a, b, c, d, top, bot;
            sx = ((fx * fx >> 8) * (768 - 2 * fx)) >> 8; /* smoothstep 3t^2 - 2t^3 on 0..256 */
            sy = ((fy * fy >> 8) * (768 - 2 * fy)) >> 8;
            a = (int32_t)(hash2((uint32_t)lx, (uint32_t)ly) & 255u) - 128;
            b = (int32_t)(hash2((uint32_t)((lx + 1) & 7), (uint32_t)ly) & 255u) - 128;
            c = (int32_t)(hash2((uint32_t)lx, (uint32_t)((ly + 1) & 7)) & 255u) - 128;
            d = (int32_t)(hash2((uint32_t)((lx + 1) & 7), (uint32_t)((ly + 1) & 7)) & 255u) - 128;
            top = (a * (256 - sx) + b * sx) >> 8;
            bot = (c * (256 - sx) + d * sx) >> 8;
            psi[y * NZ + x] = (int16_t)((top * (256 - sy) + bot * sy) >> 8);
        }
    }
    for (y = 0; y < NZ; y++) {
        for (x = 0; x < NZ; x++) {
            int32_t m = NZ - 1;
            int32_t dx = psi[y * NZ + ((x + 1) & m)] - psi[y * NZ + ((x - 1) & m)];
            int32_t dy = psi[((y + 1) & m) * NZ + x] - psi[((y - 1) & m) * NZ + x];
            s->noise[0][y * NZ + x] = (int8_t)cl(dy, -127, 127);
            s->noise[1][y * NZ + x] = (int8_t)cl(-dx, -127, 127);
        }
    }
}

void krk_fx_init(krk_fx* s) {
    memset(s, 0, sizeof *s);
    s->rng = 0x1234567u;
    noise_build(s);
}

/* ---- inputs: the only float code, once per frame ---- */
void krk_fx_inputs(krk_fx_in* in, krk_fx_mode mode, const krk_params* p, const krk_audio* a, float t, float dt,
                   uint32_t frame) {
    krk_grid g = krk_fx_grid();
    krk_brush b;
    float dd = dt * dt, lx, ly;
    memset(in, 0, sizeof *in);
    krk_brush_at(&b, t, &g, p, a);
    in->mode = (int32_t)mode;
    in->bx = cl((int32_t)(b.pos[0] * 256.0f), 0, (W - 1) << 8);
    in->by = cl((int32_t)(b.pos[1] * 256.0f), 0, (H - 1) << 8);
    in->brad = cl((int32_t)(b.radius * 256.0f), 256, 2600);
    in->bvx = cl((int32_t)(b.vel[0] * dt * 256.0f), -VMAX, VMAX);
    in->bvy = cl((int32_t)(b.vel[1] * dt * 256.0f), -VMAX, VMAX);
    in->brush_on = b.active;
    in->emis = cl((int32_t)(9.0f * a->emission * dt * 65536.0f), 0, 65535);
    in->heat = cl((int32_t)(9.0f * a->heat_mul * dt * 65536.0f), 0, 65535);
    in->push = cl((int32_t)(80.0f * a->force * (1.0f + 4.0f * a->pump)), -2000, 2000);
    in->lift = cl((int32_t)(0.9f * p->v[KRK_K_LIFT] * (float)H * dd * 256.0f + 0.5f), 0, 255);
    in->turb = cl((int32_t)(0.3f * p->v[KRK_K_TURBULENCE] * a->turb_mul * (float)H * dd * 256.0f + 0.5f), 0, 255);
    in->conf = cl((int32_t)(a->swirl * 4.0f), 0, 400);
    in->dec_smoke = cl((int32_t)(expf(-dt / (2.2f * p->v[KRK_K_LIFE])) * 65536.0f), 0, 65535);
    in->dec_heat = cl((int32_t)(expf(-dt / p->v[KRK_K_COOLTIME]) * 65536.0f), 0, 65535);
    in->dec_vel = cl((int32_t)(expf(-0.2f * dt) * 65536.0f), 0, 65535);
    in->jacobi = (int32_t)g.jacobi;
    in->life_frames = cl((int32_t)(1.2f * p->v[KRK_K_LIFE] / dt), 4, 250);
    in->show_particles = p->v[KRK_K_SHOWPARTICLES] >= 0.5f;
    in->show_smoke = p->v[KRK_K_SHOWSMOKE] >= 0.5f;
    if (mode == KRK_FX_LAVA) {
        in->nparticles = cl((int32_t)((float)KRK_FX_PARTICLES * p->v[KRK_K_PARTICLES]), 0, (int32_t)KRK_FX_MAXP);
        in->smoke_gain = cl((int32_t)(400.0f * p->v[KRK_K_SMOKE]), 0, 600);
    } else {
        in->nparticles = 0;
        in->show_particles = 0;
        in->show_smoke = 1;
        in->smoke_gain = 384;
    }
    in->spawn_x = in->bx << 10; /* cell 8.8 -> 160x128 pixels 16.16 (x4 per cell) */
    in->spawn_y = in->by << 10;
    in->light_gain = cl((int32_t)(256.0f * p->v[KRK_K_LIGHT] * (1.0f + 2.5f * a->flash)), 0, 1023);
    in->shadow_k = cl((int32_t)(64.0f * p->v[KRK_K_SHADOWS]), 0, 255);
    in->bloom_gain = cl((int32_t)(256.0f * p->v[KRK_K_BLOOM] * (1.0f + 3.0f * a->flash)), 0, 2047);
    if (p->v[KRK_K_CURSORLIGHT] >= 0.5f) {
        lx = b.pos[0];
        ly = b.pos[1] + 0.35f * (float)H;
    } else {
        lx = 0.45f * (float)W;
        ly = 0.85f * (float)H;
    }
    lx -= 0.5f * (float)W;
    ly -= 0.5f * (float)H;
    in->ldx = lx > 1.5f ? 1 : (lx < -1.5f ? -1 : 0);
    in->ldy = ly > 1.5f ? 1 : (ly < -1.5f ? -1 : 0);
    if (in->ldx == 0 && in->ldy == 0) {
        in->ldy = 1;
    }
    in->noise_t = (int32_t)frame;
    in->frame = frame;
}

/* ---- simulation ----
 * Velocity is always inside +-VMAX (every write clamps), so a zero increment is skipped: the clamp would be an
 * identity. The output of every stage is bit-identical to the plain per-cell form (tests/viz/krk_lava_test.c golden). */
static void splat(krk_fx* s, const krk_fx_in* in) {
    int32_t x0 = cl((in->bx - in->brad) >> 8, 0, W - 1), x1 = cl(((in->bx + in->brad) >> 8) + 1, 0, W - 1);
    int32_t y0 = cl((in->by - in->brad) >> 8, 0, H - 1), y1 = cl(((in->by + in->brad) >> 8) + 1, 0, H - 1), x, y;
    int32_t r2 = in->brad * in->brad;
    if (!in->brush_on) {
        return;
    }
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) {
            int32_t dx = (x << 8) - in->bx, dy = (y << 8) - in->by, d2 = dx * dx + dy * dy, w, i = y * W + x, sm, ht;
            if (d2 >= r2) {
                continue;
            }
            /* d2 < r2: below 2^24 the quotient fits a 32-bit divide (no 64-bit library divide on 68k) */
            w = 256 - (r2 < (1 << 24) ? (int32_t)(((uint32_t)d2 << 8) / (uint32_t)r2)
                                      : (int32_t)(((int64_t)d2 * 256) / r2));
            s->u[i] = (int16_t)cl(s->u[i] + (((in->bvx - s->u[i]) * w) >> 10) + ((dx * in->push >> 8) * w >> 12),
                                  -VMAX, VMAX);
            s->v[i] = (int16_t)cl(s->v[i] + (((in->bvy - s->v[i]) * w) >> 10) + ((dy * in->push >> 8) * w >> 12),
                                  -VMAX, VMAX);
            sm = s->smoke[i] + ((in->emis * w) >> 8);
            ht = s->heat[i] + ((in->heat * w) >> 8);
            s->smoke[i] = (uint16_t)cl(sm, 0, 65535);
            s->heat[i] = (uint16_t)cl(ht, 0, 65535);
        }
    }
}

static void forces(krk_fx* s, const krk_fx_in* in) {
    const int32_t lift = in->lift, turb = in->turb, smoke2d = in->mode == KRK_FX_SMOKE2D, nx = in->noise_t >> 1;
    int32_t x, y, i = 0;
    for (y = 0; y < H; y++) {
        const int32_t nrow = ((y + (in->noise_t >> 2)) & (NZ - 1)) * NZ;
        const int8_t *n0 = s->noise[0] + nrow, *n1 = s->noise[1] + nrow;
        for (x = 0; x < W; x++, i++) {
            int32_t sm = s->smoke[i], ht = s->heat[i], gate, dv;
            if (!(sm | ht)) {
                continue; /* empty cell: no buoyancy, turbulence gated to zero */
            }
            gate = (sm + ht) >> 9;
            dv = (ht * lift) >> 16;
            if (smoke2d) {
                dv += (sm * lift) >> 17;
            }
            if (gate && turb) {
                int32_t ni = (x + nx) & (NZ - 1), du = (((int32_t)n0[ni] * turb) >> 7) * gate >> 8;
                dv += (((int32_t)n1[ni] * turb) >> 7) * gate >> 8;
                if (du) {
                    s->u[i] = (int16_t)cl(s->u[i] + du, -VMAX, VMAX);
                }
            }
            if (dv) {
                s->v[i] = (int16_t)cl(s->v[i] + dv, -VMAX, VMAX);
            }
        }
    }
}

static void vorticity(krk_fx* s, const krk_fx_in* in) {
    const int32_t conf = in->conf;
    int32_t x, y;
    /* |curl| <= 4 * VMAX, inside int16 */
    STENCIL(s->curl[i] = (int16_t)((s->v[ir] - s->v[il]) - (s->u[i + op] - s->u[i + om]));)
    if (!conf) {
        return;
    }
    for (y = 1; y < H - 1; y++) {
        const int16_t* cr = s->curl + y * W;
        for (x = 1; x < W - 1; x++) {
            int32_t c = cr[x], gx, gy, nrm, du, dv;
            if (!c) {
                continue;
            }
            gx = iabs(cr[x + 1]) - iabs(cr[x - 1]);
            gy = iabs(cr[x + W]) - iabs(cr[x - W]);
            if (!(gx | gy)) {
                continue;
            }
            nrm = iabs(gx) + iabs(gy) + 1; /* the two divides only run where the curl has a gradient */
            du = (((gy * 256 / nrm) * c >> 8) * conf) >> 12;
            dv = -((((gx * 256 / nrm) * c >> 8) * conf) >> 12);
            if (du) {
                s->u[y * W + x] = (int16_t)cl(s->u[y * W + x] + du, -VMAX, VMAX);
            }
            if (dv) {
                s->v[y * W + x] = (int16_t)cl(s->v[y * W + x] + dv, -VMAX, VMAX);
            }
        }
    }
}

static void project(krk_fx* s, int32_t iters) {
    int32_t y, k;
    int16_t *a = s->p, *b = s->t0;
    const int16_t* dv = s->div;
    STENCIL(int32_t d = ((s->u[ir] - s->u[il]) + (s->v[i + op] - s->v[i + om])) * 8;
            s->div[i] = (int16_t)cl(d, -32767, 32767);
            s->p[i] = (int16_t)((s->p[i] * 13 + 8) >> 4);) /* warm start 0.8125 */
    for (k = 0; k < iters; k++) {
        int16_t* t;
        STENCIL(int32_t sum = a[il] + a[ir] + a[i + om] + a[i + op];
                b[i] = (int16_t)cl((sum - dv[i] + 2) >> 2, -32767, 32767);)
        t = a;
        a = b;
        b = t;
    }
    if (a != s->p) {
        memcpy(s->p, a, sizeof s->p);
    }
    a = s->p;
    STENCIL(int32_t du = -((a[ir] - a[il] + 16) >> 5); int32_t dw = -((a[i + op] - a[i + om] + 16) >> 5);
            if (du) { s->u[i] = (int16_t)cl(s->u[i] + du, -VMAX, VMAX); }
            if (dw) { s->v[i] = (int16_t)cl(s->v[i] + dw, -VMAX, VMAX); })
}

static void advect_velocity(krk_fx* s, const krk_fx_in* in) {
    const int32_t dec = in->dec_vel;
    int32_t x, y, i = 0;
    bil_pos b;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++, i++) {
            int32_t u = s->u[i], v = s->v[i];
            if (!(u | v)) { /* a cell at rest samples itself: zero stays zero */
                s->t0[i] = 0;
                s->t1[i] = 0;
                continue;
            }
            bil_setup(&b, (x << 8) - u, (y << 8) - v);
            s->t0[i] = (int16_t)cl((bil_s(s->u, &b) * dec + 32768) >> 16, -VMAX, VMAX);
            s->t1[i] = (int16_t)cl((bil_s(s->v, &b) * dec + 32768) >> 16, -VMAX, VMAX);
        }
    }
    memcpy(s->u, s->t0, sizeof s->u);
    memcpy(s->v, s->t1, sizeof s->v);
    for (y = 0; y < H; y++) { /* walls left and right, floor */
        s->u[y * W] = 0;
        s->u[y * W + W - 1] = 0;
    }
    for (x = 0; x < W; x++) {
        s->v[x] = 0;
    }
}

/* Semi-Lagrangian advection with decay of heat and smoke in one pass (both sample the same back-traced point);
 * MacCormack correction on the smoke in Smoke 2D (design 3.3). Scratch: dtmp holds four 40x32 fields (the 80x64
 * density buffer is free during the step). */
static void advect_scalars(krk_fx* s, const krk_fx_in* in) {
    uint16_t *ts = s->dtmp, *t2 = s->dtmp + KRK_FX_CELLS, *th = s->dtmp + 2 * KRK_FX_CELLS;
    const uint32_t dh = (uint32_t)in->dec_heat, ds = (uint32_t)in->dec_smoke;
    const int32_t mac = in->mode == KRK_FX_SMOKE2D;
    int32_t x, y, i = 0;
    bil_pos b;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++, i++) {
            int32_t u = s->u[i], v = s->v[i];
            uint32_t hs, ss;
            if (!(u | v)) {
                hs = s->heat[i];
                ss = s->smoke[i];
            } else {
                bil_setup(&b, (x << 8) - u, (y << 8) - v);
                hs = (uint16_t)bil_u(s->heat, &b);
                ss = (uint16_t)bil_u(s->smoke, &b);
            }
            th[i] = (uint16_t)((hs * dh + 32768u) >> 16);
            if (mac) {
                ts[i] = (uint16_t)ss;
            } else {
                t2[i] = (uint16_t)((ss * ds + 32768u) >> 16);
            }
        }
    }
    if (mac) {
        for (i = 0, y = 0; y < H; y++) {
            for (x = 0; x < W; x++, i++) {
                int32_t u = s->u[i], v = s->v[i], back, r;
                if (!(u | v)) {
                    back = ts[i];
                } else {
                    bil_setup(&b, (x << 8) + u, (y << 8) + v);
                    back = bil_u(ts, &b);
                }
                r = cl(ts[i] + (((int32_t)s->smoke[i] - back) >> 1), 0, 65535);
                t2[i] = (uint16_t)(((uint32_t)r * ds + 32768u) >> 16);
            }
        }
    }
    memcpy(s->heat, th, sizeof s->heat);
    memcpy(s->smoke, t2, sizeof s->smoke);
}

static uint32_t rnd(krk_fx* s) {
    s->rng = s->rng * 1664525u + 1013904223u;
    return s->rng >> 8;
}

static void particles(krk_fx* s, const krk_fx_in* in) {
    int32_t n = in->nparticles, k, i, ns, age_max = in->life_frames * 10;
    int32_t inv = age_max > 0 ? (255 << 16) / age_max : 0;
    const int32_t turb = in->turb, nty = in->noise_t >> 2, ntx = in->noise_t >> 1;
    uint32_t plotted = 0;
    bil_pos b;
    memset(s->accum, 0, sizeof s->accum);
    if (n <= 0) {
        return;
    }
    if (in->brush_on) { /* ring-buffer respawn (reference pUpdate) */
        ns = (n + in->life_frames - 1) / in->life_frames;
        for (k = 0; k < ns; k++) {
            krk_particle* q = &s->part[s->cursor % (uint32_t)n];
            uint32_t r = rnd(s);
            int32_t jx = (((int32_t)(r & 255u) - 128) * (in->brad << 2)) >> 7;
            int32_t jy = (((int32_t)((r >> 8) & 255u) - 128) * (in->brad << 2)) >> 7;
            q->x = in->spawn_x + jx * 256;
            q->y = in->spawn_y + jy * 256;
            q->age = 0;
            q->hue = (uint8_t)(r >> 16);
            q->alive = 1;
            s->cursor++;
        }
        if (s->cursor >= 0x40000000u) {
            s->cursor %= (uint32_t)n;
        }
    }
    for (i = 0; i < n; i++) {
        krk_particle* q = &s->part[i];
        int32_t u1, v1, u2, v2, ni, val, px, py;
        uint8_t* acc;
        if (!q->alive) {
            continue;
        }
        /* positions are 16.16 pixels: >> 10 gives 8.8 cell coordinates (4 pixels per cell) */
        bil_setup(&b, q->x >> 10, q->y >> 10);
        u1 = bil_s(s->u, &b);
        v1 = bil_s(s->v, &b);
        bil_setup(&b, (q->x + u1 * 512) >> 10, (q->y + v1 * 512) >> 10); /* RK2 midpoint */
        u2 = bil_s(s->u, &b);
        v2 = bil_s(s->v, &b);
        ni = ((((q->y >> 16) + nty) & (NZ - 1)) * NZ) + (((q->x >> 16) + ntx) & (NZ - 1));
        /* 8.8 cells per frame -> 16.16 pixels: x4 pixels per cell, x256 */
        q->x += u2 * 1024 + ((int32_t)s->noise[0][ni] * turb) * 16;
        q->y += v2 * 1024 + ((int32_t)s->noise[1][ni] * turb) * 16;
        q->age = (uint16_t)(q->age + 10u);
        px = q->x >> 16;
        py = q->y >> 16;
        if (q->x < 0 || q->y < 0 || px >= OW || py >= OH || (int32_t)q->age > age_max) {
            q->alive = 0;
            continue;
        }
        val = 255 - (int32_t)(((uint32_t)q->age * (uint32_t)inv) >> 16);
        val = (val * (128 + (q->hue >> 1))) >> 9;
        acc = s->accum + py * OW + px;
        *acc = (uint8_t)cl(*acc + val, 0, 255);
        plotted++;
    }
    s->particles_plotted += plotted;
}

/* Stage timer lap: charges the ticks since t0 to stage `id` and restarts t0. Unsigned wrap is correct for one lap. */
#define KRK_LAP(s, t0, id)                          \
    do {                                            \
        if ((s)->clock != NULL) {                   \
            uint32_t lap_now = (s)->clock();        \
            (s)->stage_ticks[id] += lap_now - (t0); \
            (t0) = lap_now;                         \
        }                                           \
    } while (0)

void krk_fx_step(krk_fx* s, const krk_fx_in* in) {
    uint32_t t0 = s->clock != NULL ? s->clock() : 0u;
    splat(s, in);
    forces(s, in);
    vorticity(s, in);
    KRK_LAP(s, t0, KRK_FX_T_FORCES);
    project(s, in->jacobi);
    s->jacobi_iters += (uint32_t)in->jacobi;
    KRK_LAP(s, t0, KRK_FX_T_PROJECT);
    advect_velocity(s, in);
    advect_scalars(s, in);
    KRK_LAP(s, t0, KRK_FX_T_ADVECT);
    particles(s, in);
    KRK_LAP(s, t0, KRK_FX_T_PARTICLES);
    s->frames++;
}

/* ---- render ---- */
/* Density at 80x64: smoke upsampled x2 with a half-cell offset, plus the particle accumulator 2x2. The half-cell
 * offset makes every bilinear weight 1/4 or 3/4, so the upsample is two shift-and-add passes (rows, then columns)
 * with the same rounding as the general sampler; the borders repeat the edge cell. */
static void density(krk_fx* s, const krk_fx_in* in) {
    uint16_t* hz = s->dtmp; /* 32 rows of 80: smoke upsampled along x */
    const int32_t gain = in->smoke_gain;
    int32_t x, y;
    if (in->show_smoke) {
        for (y = 0; y < H; y++) {
            const uint16_t* f = s->smoke + y * W;
            uint16_t* o = hz + y * DW;
            int32_t a = f[0], b;
            o[0] = (uint16_t)a;
            for (x = 1; x < W; x++) {
                b = f[x];
                o[2 * x - 1] = (uint16_t)((3 * a + b + 2) >> 2);
                o[2 * x] = (uint16_t)((a + 3 * b + 2) >> 2);
                a = b;
            }
            o[DW - 1] = (uint16_t)a;
        }
    }
    for (y = 0; y < DH; y++) {
        const int32_t k = y >> 1, kb = (y & 1) ? (k + 1 < H ? k + 1 : k) : (k > 0 ? k - 1 : 0);
        const uint16_t *ra = hz + k * DW, *rb = hz + kb * DW; /* near row weight 3, far row weight 1 */
        const uint8_t* q = s->accum + (y * 2) * OW;
        uint16_t* o = s->dens + y * DW;
        for (x = 0; x < DW; x++) {
            int32_t d = 0;
            if (in->show_smoke) {
                d = (((3 * (int32_t)ra[x] + rb[x] + 2) >> 2) * gain) >> 16;
            }
            if (in->show_particles) {
                d += (q[2 * x] + q[2 * x + 1] + q[2 * x + OW] + q[2 * x + OW + 1]) >> 2;
            }
            o[x] = (uint16_t)cl(d, 0, 255);
        }
    }
}

static void blur_h(uint16_t* dst, const uint16_t* src) {
    int32_t x, y;
    for (y = 0; y < DH; y++, src += DW, dst += DW) {
        int32_t a = src[0], b = src[0], c;
        for (x = 0; x < DW - 1; x++) {
            c = src[x + 1];
            dst[x] = (uint16_t)((a + 2 * b + c) >> 2);
            a = b;
            b = c;
        }
        dst[DW - 1] = (uint16_t)((a + 3 * b) >> 2);
    }
}

static void blur_v(uint16_t* dst, const uint16_t* src) {
    int32_t x, y;
    for (y = 0; y < DH; y++, src += DW, dst += DW) {
        const uint16_t *up = y > 0 ? src - DW : src, *dn = y < DH - 1 ? src + DW : src;
        for (x = 0; x < DW; x++) {
            dst[x] = (uint16_t)((up[x] + 2 * src[x] + dn[x]) >> 2);
        }
    }
}

/* One light cell: T is the light of the neighbour toward the source thinned by that neighbour's density, shaded by
 * the density difference across the cell. Out-of-grid neighbours: full light, no density. */
#define LIGHT_PX(TX_OK, AX_OK)                                                                                         \
    {                                                                                                                  \
        int32_t T = 255, toward = 0, away = 0;                                                                         \
        if (TX_OK) {                                                                                                   \
            toward = td[x + ldx];                                                                                      \
            T = (tl[x + ldx] * (int32_t)thin[toward]) >> 8;                                                            \
        }                                                                                                              \
        if (AX_OK) {                                                                                                   \
            away = ad[x - ldx];                                                                                        \
        }                                                                                                              \
        o[x] = (uint8_t)((T * ((cl(128 + ((away - toward) >> 1), 0, 255) >> 1) + 128)) >> 8);                          \
    }

static void light_sweep(krk_fx* s, const krk_fx_in* in) {
    static const uint16_t zero_row[KRK_FX_DW];
    const int32_t ldx = in->ldx, ldy = in->ldy;
    int32_t xs = ldx > 0 ? DW - 1 : 0, xd = ldx > 0 ? -1 : 1, xl = ldx > 0 ? 0 : DW - 1;
    int32_t ys = ldy > 0 ? DH - 1 : 0, ye = ldy > 0 ? -1 : DH, yd = ldy > 0 ? -1 : 1, x, y;
    uint16_t thin[256]; /* 256 - shadow opacity per density */
    uint8_t full_row[KRK_FX_DW];
    for (x = 0; x < 256; x++) {
        thin[x] = (uint16_t)(256 - cl((x * in->shadow_k) >> 7, 0, 255));
    }
    memset(full_row, 255, sizeof full_row);
    for (y = ys; y != ye; y += yd) {
        const int32_t ty = y + ldy, ay = y - ldy;
        const int32_t t_in = ty >= 0 && ty < DH, a_in = ay >= 0 && ay < DH;
        const uint16_t* td = t_in ? s->dens + ty * DW : zero_row; /* a row off the grid: density 0, light 255 */
        const uint8_t* tl = t_in ? s->light + ty * DW : full_row;
        const uint16_t* ad = a_in ? s->dens + ay * DW : zero_row;
        uint8_t* o = s->light + y * DW;
        x = xs;
        LIGHT_PX(t_in && x + ldx >= 0 && x + ldx < DW, x - ldx >= 0 && x - ldx < DW)
        for (x = xs + xd; x != xl; x += xd) {
            LIGHT_PX(1, 1)
        }
        LIGHT_PX(t_in && x + ldx >= 0 && x + ldx < DW, x - ldx >= 0 && x - ldx < DW)
    }
}

/* Bloom: 3x3 box of heat at 40x32, as a row sum then a column sum (edges repeat). */
static void bloom(krk_fx* s) {
    uint16_t* hs = s->dtmp;
    int32_t x, y;
    for (y = 0; y < H; y++) {
        const uint16_t* h = s->heat + y * W;
        uint16_t* o = hs + y * W;
        int32_t a = h[0] >> 8, b = a, c;
        for (x = 0; x < W - 1; x++) {
            c = h[x + 1] >> 8;
            o[x] = (uint16_t)(a + b + c);
            a = b;
            b = c;
        }
        o[W - 1] = (uint16_t)(a + 2 * b);
    }
    for (y = 0; y < H; y++) {
        const uint16_t *mid = hs + y * W, *up = y > 0 ? mid - W : mid, *dn = y < H - 1 ? mid + W : mid;
        uint8_t* o = s->bloom + y * W;
        for (x = 0; x < W; x++) {
            o[x] = (uint8_t)cl(((up[x] + mid[x] + dn[x]) * 57) >> 9, 0, 255);
        }
    }
}

static const uint8_t k_bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

/* Composite to 160x128. Lit smoke, heat tint and bloom only change per 80x64 density cell, so they are summed once
 * per cell; each of the four pixels of a cell adds the background gradient, the dither and its particle value. */
static void composite(const krk_fx* s, const krk_fx_in* in, uint8_t* out) {
    /* Saturation by table instead of two compares per pixel: the index is cell value (capped at 511, which is
     * already white with any gradient, dither and particle value) + gradient/dither (-2..24) + particles (0..255),
     * biased by SAT_BIAS so it never goes below zero. */
    enum { SAT_BIAS = 8, SAT_N = 512 + 32 + 256 };
    static uint8_t sat[SAT_N];
    const uint8_t* const satb = sat + SAT_BIAS;
    uint16_t cellv[KRK_FX_DW];
    int32_t blm[KRK_FX_W];
    uint8_t fac[256]; /* light -> smoke brightness */
    int32_t x, dy, r;
    if (!sat[SAT_N - 1]) {
        for (x = 0; x < SAT_N; x++) {
            sat[x] = (uint8_t)cl(x - SAT_BIAS, 0, 255);
        }
    }
    for (x = 0; x < 256; x++) {
        fac[x] = (uint8_t)cl(96 + ((x * in->light_gain) >> 9), 0, 255);
    }
    for (dy = 0; dy < DH; dy++) {
        const uint16_t* dn = s->dens + dy * DW;
        const uint8_t* li = s->light + dy * DW;
        const uint16_t* ht = s->heat + (dy >> 1) * W;
        if (!(dy & 1)) {
            const uint8_t* bl = s->bloom + (dy >> 1) * W;
            for (x = 0; x < W; x++) {
                blm[x] = (bl[x] * in->bloom_gain) >> 10; /* bloom, tonemap baked into the ramp */
            }
        }
        for (x = 0; x < DW; x++) {
            int32_t d8 = cl(dn[x], 0, 255), h8 = ht[x >> 1] >> 8;
            int32_t c = ((d8 * fac[li[x]]) >> 8)          /* lit smoke */
                        + ((h8 * ((d8 >> 1) + 64)) >> 8) /* heat colours the smoke */
                        + blm[x >> 1];
            cellv[x] = (uint16_t)cl(c, 0, 511);
        }
        for (r = 0; r < 2; r++) {
            const int32_t oy = dy * 2 + r, g = (oy * 23) >> 7; /* background gradient */
            const uint8_t* by = k_bayer + ((oy & 3) << 2);
            const uint8_t* ac = s->accum + oy * OW;
            uint8_t* o = out + (OH - 1 - oy) * OW; /* row 0 on top, sim y points up */
            int32_t dg[4];
            for (x = 0; x < 4; x++) {
                dg[x] = g + (((int32_t)by[x] - 8) >> 2);
            }
            if (in->show_particles) {
                for (x = 0; x < OW; x += 4) { /* two cells, four dither columns per turn */
                    const uint8_t *c0 = satb + cellv[x >> 1], *c1 = satb + cellv[(x >> 1) + 1];
                    o[x] = c0[dg[0] + ac[x]];
                    o[x + 1] = c0[dg[1] + ac[x + 1]];
                    o[x + 2] = c1[dg[2] + ac[x + 2]];
                    o[x + 3] = c1[dg[3] + ac[x + 3]];
                }
            } else {
                for (x = 0; x < OW; x += 4) {
                    const uint8_t *c0 = satb + cellv[x >> 1], *c1 = satb + cellv[(x >> 1) + 1];
                    o[x] = c0[dg[0]];
                    o[x + 1] = c0[dg[1]];
                    o[x + 2] = c1[dg[2]];
                    o[x + 3] = c1[dg[3]];
                }
            }
        }
    }
}

void krk_fx_render(krk_fx* s, const krk_fx_in* in, uint8_t* out) {
    uint32_t t0 = s->clock != NULL ? s->clock() : 0u;
    density(s, in);
    KRK_LAP(s, t0, KRK_FX_T_DENSITY);
    blur_h(s->dtmp, s->dens);
    blur_v(s->dens, s->dtmp);
    blur_h(s->dtmp, s->dens);
    blur_v(s->dens, s->dtmp);
    KRK_LAP(s, t0, KRK_FX_T_BLUR);
    light_sweep(s, in);
    KRK_LAP(s, t0, KRK_FX_T_LIGHT);
    bloom(s);
    KRK_LAP(s, t0, KRK_FX_T_BLOOM);
    composite(s, in, out);
    KRK_LAP(s, t0, KRK_FX_T_COMPOSITE);
}

/* ---- palette ramp ---- */
typedef struct ramp_stop {
    int32_t at;
    const float* c;
} ramp_stop;

static void ramp_fill(uint32_t rgb[256], const ramp_stop* st, int32_t n) {
    int32_t k, i;
    for (k = 0; k + 1 < n; k++) {
        for (i = st[k].at; i <= st[k + 1].at; i++) {
            float t = (float)(i - st[k].at) / (float)(st[k + 1].at - st[k].at), c[3];
            int32_t j;
            for (j = 0; j < 3; j++) {
                c[j] = st[k].c[j] + (st[k + 1].c[j] - st[k].c[j]) * t;
            }
            rgb[i] = krk_rgb_hex(c);
        }
    }
}

/* The ramp from two palettes: `fire` gives the hot stops (hot, ember), `body` the cool ones (bg, smoke, key). Equal
 * palettes give the plain ramp; the crossfade passes the leading fire blend and the lagging body blend (krk_palfx_colors). */
static void fx_palette_from(krk_fx_mode mode, const krk_palette* fire, const krk_palette* body, uint32_t rgb[256]) {
    if (mode == KRK_FX_LAVA) {
        ramp_stop st[8];
        st[0].at = 0, st[0].c = body->bg[0];
        st[1].at = 23, st[1].c = body->bg[1];
        st[2].at = 80, st[2].c = body->smoke;
        st[3].at = 140, st[3].c = fire->hot[0];
        st[4].at = 180, st[4].c = fire->hot[1];
        st[5].at = 215, st[5].c = fire->hot[2];
        st[6].at = 240, st[6].c = fire->hot[3];
        st[7].at = 255, st[7].c = fire->ember;
        ramp_fill(rgb, st, 8);
    } else {
        float dim[3];
        ramp_stop st[5];
        dim[0] = body->smoke[0] * 0.3f;
        dim[1] = body->smoke[1] * 0.3f;
        dim[2] = body->smoke[2] * 0.3f;
        st[0].at = 0, st[0].c = body->bg[0];
        st[1].at = 23, st[1].c = body->bg[1];
        st[2].at = 90, st[2].c = dim;
        st[3].at = 180, st[3].c = body->smoke;
        st[4].at = 255, st[4].c = body->key;
        ramp_fill(rgb, st, 5);
    }
}

void krk_fx_palette(krk_fx_mode mode, const krk_params* p, uint32_t rgb[256]) {
    const krk_palette* pal = krk_palette_get((uint32_t)p->v[KRK_K_PALETTE]);
    fx_palette_from(mode, pal, pal, rgb);
}

void krk_fx_palette_fx(krk_fx_mode mode, const krk_palfx* fx, uint32_t rgb[256]) {
    krk_palette fire, body;
    krk_palfx_colors(fx, &fire, &body);
    fx_palette_from(mode, &fire, &body, rgb);
}

/* The source column and row advance by an error term, and a surface row that maps to the same source row as the one
 * above is a copy of it. */
void krk_fx_blit(const uint8_t* idx, const uint32_t* ramp, uint32_t* pixels, uint32_t pitch, uint32_t w, uint32_t h) {
    uint32_t pal[256];
    const uint32_t* prev = NULL;
    uint32_t x, y, ey = 0, sy = 0, prev_sy = 0;
    for (x = 0; x < 256u; x++) {
        pal[x] = 0xFF000000u | ramp[x];
    }
    for (y = 0; y < h; y++) {
        uint32_t* row = (uint32_t*)(void*)((uint8_t*)pixels + (size_t)y * pitch);
        if (prev != NULL && sy == prev_sy) {
            memcpy(row, prev, (size_t)w * sizeof *row);
        } else {
            const uint8_t* src = idx + (size_t)sy * KRK_FX_OW;
            uint32_t ex = 0;
            for (x = 0; x < w; x++) {
                row[x] = pal[*src];
                for (ex += KRK_FX_OW; ex >= w; ex -= w) {
                    src++;
                }
            }
            prev = row;
            prev_sy = sy;
        }
        for (ey += KRK_FX_OH; ey >= h; ey -= h) {
            sy++;
        }
    }
}
