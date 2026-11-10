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
static int32_t cell(int32_t x, int32_t y) { return cl(y, 0, H - 1) * W + cl(x, 0, W - 1); }

/* Bilinear sample at 8.8 cell coordinates, clamped to the grid. Two 8-bit weight steps keep it inside 32 bits. */
#define BILINEAR(NAME, T)                                                                                              \
    static int32_t NAME(const T* f, int32_t fx, int32_t fy) {                                                          \
        int32_t ix, iy, ax, ay, ix1, iy1, top, bot;                                                                    \
        fx = cl(fx, 0, (W - 1) << 8);                                                                                  \
        fy = cl(fy, 0, (H - 1) << 8);                                                                                  \
        ix = fx >> 8;                                                                                                  \
        iy = fy >> 8;                                                                                                  \
        ax = fx & 255;                                                                                                 \
        ay = fy & 255;                                                                                                 \
        ix1 = ix + 1 < W ? ix + 1 : ix;                                                                                \
        iy1 = iy + 1 < H ? iy + 1 : iy;                                                                                \
        top = ((int32_t)f[iy * W + ix] * (256 - ax) + (int32_t)f[iy * W + ix1] * ax + 128) >> 8;                             \
        bot = ((int32_t)f[iy1 * W + ix] * (256 - ax) + (int32_t)f[iy1 * W + ix1] * ax + 128) >> 8;                           \
        return (top * (256 - ay) + bot * ay + 128) >> 8;                                                                     \
    }
BILINEAR(bil_s, int16_t)
BILINEAR(bil_u, uint16_t)

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

/* ---- simulation ---- */
static void add_vel(krk_fx* s, int32_t i, int32_t du, int32_t dv) {
    s->u[i] = (int16_t)cl(s->u[i] + du, -VMAX, VMAX);
    s->v[i] = (int16_t)cl(s->v[i] + dv, -VMAX, VMAX);
}

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
            w = 256 - (int32_t)(((int64_t)d2 * 256) / r2);
            add_vel(s, i, (((in->bvx - s->u[i]) * w) >> 10) + ((dx * in->push >> 8) * w >> 12),
                    (((in->bvy - s->v[i]) * w) >> 10) + ((dy * in->push >> 8) * w >> 12));
            sm = s->smoke[i] + ((in->emis * w) >> 8);
            ht = s->heat[i] + ((in->heat * w) >> 8);
            s->smoke[i] = (uint16_t)cl(sm, 0, 65535);
            s->heat[i] = (uint16_t)cl(ht, 0, 65535);
        }
    }
}

static void forces(krk_fx* s, const krk_fx_in* in) {
    int32_t x, y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int32_t i = y * W + x, gate = (s->smoke[i] + s->heat[i]) >> 9;
            int32_t ni = ((y + (in->noise_t >> 2)) & (NZ - 1)) * NZ + ((x + (in->noise_t >> 1)) & (NZ - 1));
            int32_t buoy = (s->heat[i] * in->lift) >> 16, du, dv;
            if (in->mode == KRK_FX_SMOKE2D) {
                buoy += (s->smoke[i] * in->lift) >> 17;
            }
            du = (((int32_t)s->noise[0][ni] * in->turb) >> 7) * gate >> 8;
            dv = buoy + ((((int32_t)s->noise[1][ni] * in->turb) >> 7) * gate >> 8);
            add_vel(s, i, du, dv);
        }
    }
}

static void vorticity(krk_fx* s, const krk_fx_in* in) {
    int32_t x, y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int32_t c = (s->v[cell(x + 1, y)] - s->v[cell(x - 1, y)]) - (s->u[cell(x, y + 1)] - s->u[cell(x, y - 1)]);
            s->curl[y * W + x] = (int16_t)cl(c, -32767, 32767);
        }
    }
    for (y = 1; y < H - 1; y++) {
        for (x = 1; x < W - 1; x++) {
            int32_t i = y * W + x, c = s->curl[i];
            int32_t gx = iabs(s->curl[i + 1]) - iabs(s->curl[i - 1]), gy = iabs(s->curl[i + W]) - iabs(s->curl[i - W]);
            int32_t nrm = iabs(gx) + iabs(gy) + 1, nx = gx * 256 / nrm, ny = gy * 256 / nrm;
            add_vel(s, i, ((ny * c >> 8) * in->conf) >> 12, -(((nx * c >> 8) * in->conf) >> 12));
        }
    }
}

static void project(krk_fx* s, int32_t iters) {
    int32_t x, y, k;
    int16_t *a = s->p, *b = s->t0;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int32_t d = ((s->u[cell(x + 1, y)] - s->u[cell(x - 1, y)]) + (s->v[cell(x, y + 1)] - s->v[cell(x, y - 1)])) << 3;
            s->div[y * W + x] = (int16_t)cl(d, -32767, 32767);
            s->p[y * W + x] = (int16_t)((s->p[y * W + x] * 13 + 8) >> 4); /* warm start 0.8125 */
        }
    }
    for (k = 0; k < iters; k++) {
        int16_t* t;
        for (y = 0; y < H; y++) {
            for (x = 0; x < W; x++) {
                int32_t sum = a[cell(x - 1, y)] + a[cell(x + 1, y)] + a[cell(x, y - 1)] + a[cell(x, y + 1)];
                b[y * W + x] = (int16_t)cl((sum - s->div[y * W + x] + 2) >> 2, -32767, 32767);
            }
        }
        t = a;
        a = b;
        b = t;
    }
    if (a != s->p) {
        memcpy(s->p, a, sizeof s->p);
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            add_vel(s, y * W + x, -((s->p[cell(x + 1, y)] - s->p[cell(x - 1, y)] + 16) >> 5),
                    -((s->p[cell(x, y + 1)] - s->p[cell(x, y - 1)] + 16) >> 5));
        }
    }
}

static void advect_velocity(krk_fx* s, const krk_fx_in* in) {
    int32_t x, y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int32_t i = y * W + x, fx = (x << 8) - s->u[i], fy = (y << 8) - s->v[i];
            s->t0[i] = (int16_t)cl((bil_s(s->u, fx, fy) * in->dec_vel + 32768) >> 16, -VMAX, VMAX);
            s->t1[i] = (int16_t)cl((bil_s(s->v, fx, fy) * in->dec_vel + 32768) >> 16, -VMAX, VMAX);
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

/* Semi-Lagrangian scalar advection with decay; MacCormack correction when mac is set (Smoke 2D, design 3.3).
 * Scratch: dtmp holds two 40x32 fields (the 80x64 density buffer is free during the step). */
static void advect_scalar(krk_fx* s, uint16_t* f, int32_t decay, int32_t mac) {
    uint16_t* t = s->dtmp;
    uint16_t* t2 = s->dtmp + KRK_FX_CELLS;
    int32_t x, y;
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int32_t i = y * W + x;
            t[i] = (uint16_t)bil_u(f, (x << 8) - s->u[i], (y << 8) - s->v[i]);
        }
    }
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int32_t i = y * W + x, r = t[i];
            if (mac) {
                int32_t back = bil_u(t, (x << 8) + s->u[i], (y << 8) + s->v[i]);
                r = cl(r + (((int32_t)f[i] - back) >> 1), 0, 65535);
            }
            t2[i] = (uint16_t)(((uint32_t)r * (uint32_t)decay + 32768u) >> 16);
        }
    }
    memcpy(f, t2, sizeof(uint16_t) * KRK_FX_CELLS);
}

static uint32_t rnd(krk_fx* s) {
    s->rng = s->rng * 1664525u + 1013904223u;
    return s->rng >> 8;
}

static void particles(krk_fx* s, const krk_fx_in* in) {
    int32_t n = in->nparticles, k, i, ns, age_max = in->life_frames * 10;
    int32_t inv = age_max > 0 ? (255 << 16) / age_max : 0;
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
            q->x = in->spawn_x + (jx << 8);
            q->y = in->spawn_y + (jy << 8);
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
        int32_t u1, v1, u2, v2, ni, val, px, py, cx, cy;
        if (!q->alive) {
            continue;
        }
        /* positions are 16.16 pixels: >> 10 gives 8.8 cell coordinates (4 pixels per cell) */
        u1 = bil_s(s->u, q->x >> 10, q->y >> 10);
        v1 = bil_s(s->v, q->x >> 10, q->y >> 10);
        cx = (q->x + (u1 << 9)) >> 10;
        cy = (q->y + (v1 << 9)) >> 10;
        u2 = bil_s(s->u, cx, cy); /* RK2 midpoint */
        v2 = bil_s(s->v, cx, cy);
        ni = ((((q->y >> 16) + (in->noise_t >> 2)) & (NZ - 1)) * NZ) + (((q->x >> 16) + (in->noise_t >> 1)) & (NZ - 1));
        /* 8.8 cells per frame -> 16.16 pixels: x4 pixels per cell, x256 */
        q->x += (u2 << 10) + (((int32_t)s->noise[0][ni] * in->turb) << 4);
        q->y += (v2 << 10) + (((int32_t)s->noise[1][ni] * in->turb) << 4);
        q->age = (uint16_t)(q->age + 10u);
        px = q->x >> 16;
        py = q->y >> 16;
        if (q->x < 0 || q->y < 0 || px >= OW || py >= OH || (int32_t)q->age > age_max) {
            q->alive = 0;
            continue;
        }
        val = 255 - (int32_t)(((uint32_t)q->age * (uint32_t)inv) >> 16);
        val = (val * (128 + (q->hue >> 1))) >> 9;
        s->accum[py * OW + px] = (uint8_t)cl(s->accum[py * OW + px] + val, 0, 255);
        s->particles_plotted++;
    }
}

void krk_fx_step(krk_fx* s, const krk_fx_in* in) {
    splat(s, in);
    forces(s, in);
    vorticity(s, in);
    project(s, in->jacobi);
    s->jacobi_iters += (uint32_t)in->jacobi;
    advect_velocity(s, in);
    advect_scalar(s, s->heat, in->dec_heat, 0);
    advect_scalar(s, s->smoke, in->dec_smoke, in->mode == KRK_FX_SMOKE2D);
    particles(s, in);
    s->frames++;
}

/* ---- render ---- */
static void blur_h(uint16_t* dst, const uint16_t* src) {
    int32_t x, y;
    for (y = 0; y < DH; y++) {
        const uint16_t* r = src + y * DW;
        for (x = 0; x < DW; x++) {
            dst[y * DW + x] = (uint16_t)((r[x > 0 ? x - 1 : 0] + 2 * r[x] + r[x < DW - 1 ? x + 1 : x]) >> 2);
        }
    }
}

static void blur_v(uint16_t* dst, const uint16_t* src) {
    int32_t x, y;
    for (y = 0; y < DH; y++) {
        for (x = 0; x < DW; x++) {
            dst[y * DW + x] = (uint16_t)((src[(y > 0 ? y - 1 : 0) * DW + x] + 2 * src[y * DW + x] +
                                          src[(y < DH - 1 ? y + 1 : y) * DW + x]) >> 2);
        }
    }
}

static void light_sweep(krk_fx* s, const krk_fx_in* in) {
    int32_t xs = in->ldx > 0 ? DW - 1 : 0, xe = in->ldx > 0 ? -1 : DW, xd = in->ldx > 0 ? -1 : 1;
    int32_t ys = in->ldy > 0 ? DH - 1 : 0, ye = in->ldy > 0 ? -1 : DH, yd = in->ldy > 0 ? -1 : 1, x, y;
    for (y = ys; y != ye; y += yd) {
        for (x = xs; x != xe; x += xd) {
            int32_t tx = x + in->ldx, ty = y + in->ldy, ax = x - in->ldx, ay = y - in->ldy, T, f, toward, away;
            if (tx < 0 || tx >= DW || ty < 0 || ty >= DH) {
                T = 255;
                toward = 0;
            } else {
                int32_t d = cl(s->dens[ty * DW + tx], 0, 255), a = cl((d * in->shadow_k) >> 7, 0, 255);
                T = (s->light[ty * DW + tx] * (256 - a)) >> 8;
                toward = d;
            }
            away = (ax < 0 || ax >= DW || ay < 0 || ay >= DH) ? 0 : cl(s->dens[ay * DW + ax], 0, 255);
            f = cl(128 + ((away - toward) >> 1), 0, 255);
            s->light[y * DW + x] = (uint8_t)((T * ((f >> 1) + 128)) >> 8);
        }
    }
}

static const uint8_t k_bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

void krk_fx_render(krk_fx* s, const krk_fx_in* in, uint8_t* out) {
    int32_t x, y;
    /* density at 80x64: smoke upsampled (half-cell offset) plus the particle accumulator 2x2 */
    for (y = 0; y < DH; y++) {
        for (x = 0; x < DW; x++) {
            int32_t d = 0;
            if (in->show_smoke) {
                d = (bil_u(s->smoke, x * 128 - 64, y * 128 - 64) * in->smoke_gain) >> 16;
            }
            if (in->show_particles) {
                const uint8_t* q = s->accum + (y * 2) * OW + x * 2;
                d += (q[0] + q[1] + q[OW] + q[OW + 1]) >> 2;
            }
            s->dens[y * DW + x] = (uint16_t)cl(d, 0, 255);
        }
    }
    blur_h(s->dtmp, s->dens);
    blur_v(s->dens, s->dtmp);
    blur_h(s->dtmp, s->dens);
    blur_v(s->dens, s->dtmp);
    light_sweep(s, in);
    for (y = 0; y < H; y++) { /* bloom: 3x3 box of heat at 40x32 */
        for (x = 0; x < W; x++) {
            int32_t sum = 0, dx, dy;
            for (dy = -1; dy <= 1; dy++) {
                for (dx = -1; dx <= 1; dx++) {
                    sum += s->heat[cell(x + dx, y + dy)] >> 8;
                }
            }
            s->bloom[y * W + x] = (uint8_t)cl((sum * 57) >> 9, 0, 255);
        }
    }
    for (y = 0; y < OH; y++) {
        int32_t oy = OH - 1 - y; /* row 0 on top, sim y points up */
        for (x = 0; x < OW; x++) {
            int32_t di = (oy >> 1) * DW + (x >> 1), ci = (oy >> 2) * W + (x >> 2);
            int32_t d8 = cl(s->dens[di], 0, 255), l = s->light[di], h8 = s->heat[ci] >> 8, v;
            int32_t fac = cl(96 + ((l * in->light_gain) >> 9), 0, 255);
            v = (oy * 23) >> 7;                         /* background gradient */
            v += (d8 * fac) >> 8;                       /* lit smoke */
            v += (h8 * ((d8 >> 1) + 64)) >> 8;          /* heat colours the smoke */
            v += (s->bloom[ci] * in->bloom_gain) >> 10; /* bloom, tonemap baked into the ramp */
            if (in->show_particles) {
                v += s->accum[oy * OW + x];
            }
            v += ((int32_t)k_bayer[((oy & 3) << 2) | (x & 3)] - 8) >> 2;
            out[y * OW + x] = (uint8_t)cl(v, 0, 255);
        }
    }
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

void krk_fx_palette(krk_fx_mode mode, const krk_params* p, uint32_t rgb[256]) {
    const krk_palette* pal = krk_palette_get((uint32_t)p->v[KRK_K_PALETTE]);
    if (mode == KRK_FX_LAVA) {
        ramp_stop st[8];
        st[0].at = 0, st[0].c = pal->bg[0];
        st[1].at = 23, st[1].c = pal->bg[1];
        st[2].at = 80, st[2].c = pal->smoke;
        st[3].at = 140, st[3].c = pal->hot[0];
        st[4].at = 180, st[4].c = pal->hot[1];
        st[5].at = 215, st[5].c = pal->hot[2];
        st[6].at = 240, st[6].c = pal->hot[3];
        st[7].at = 255, st[7].c = pal->ember;
        ramp_fill(rgb, st, 8);
    } else {
        float dim[3];
        ramp_stop st[5];
        dim[0] = pal->smoke[0] * 0.3f;
        dim[1] = pal->smoke[1] * 0.3f;
        dim[2] = pal->smoke[2] * 0.3f;
        st[0].at = 0, st[0].c = pal->bg[0];
        st[1].at = 23, st[1].c = pal->bg[1];
        st[2].at = 90, st[2].c = dim;
        st[3].at = 180, st[3].c = pal->smoke;
        st[4].at = 255, st[4].c = pal->key;
        ramp_fill(rgb, st, 5);
    }
}
