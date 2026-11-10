/* Krakatoa fixed-point Lava core (I1052b, KRK-P0-4 / KRK-P0-5; design thoughts/shared/research/2026-10-27-krakatoa-port-design.md
 * sections 3.2, 3.3, 3.6, 3.7). Portable C99, integer only inside the frame: no float, no libm in krk_fx_step and
 * krk_fx_render, so a 68030 without FPU runs the same code as a 68060 (design 3.6). The per-frame floats (knobs, audio
 * mapper, brush) are converted once by krk_fx_inputs, which drives the existing krk_core params, palettes and mapper.
 *
 * Sim grid 40x32 cells (y up). Formats (design 3.6): velocity int16 8.8 cells per frame, clamped to +-4 cells; heat and
 * smoke uint16 0.16; pressure and divergence int16 4.12 with 32-bit accumulators; particles x, y 16.16 on the 160x128
 * screen, age 8.8 s, hue 8 bit. Density and light run at 80x64, bloom at 40x32, the composite writes 160x128 palette
 * indices (0..255) with an ordered dither; krk_fx_palette bakes the 256 colours from the knob's krk_palette.
 * The hot loops (Jacobi, advection, particles, blur, light, composite) are the future 68k asm twins (design 3.7); this C
 * is their reference path and keeps their signatures simple: plain arrays, no globals. */
#ifndef KRK_LAVA_FX_H
#define KRK_LAVA_FX_H

#include <stdint.h>

#include "krk_core.h"

#define KRK_FX_W 40u
#define KRK_FX_H 32u
#define KRK_FX_CELLS (KRK_FX_W * KRK_FX_H)
#define KRK_FX_DW 80u
#define KRK_FX_DH 64u
#define KRK_FX_OW 160u
#define KRK_FX_OH 128u
#define KRK_FX_JACOBI 8u
#define KRK_FX_PARTICLES 3000u /* 060 tier count (design 5); the knob `particles` scales it */
#define KRK_FX_MAXP 6000u
#define KRK_FX_NOISE 64u

typedef enum krk_fx_mode { KRK_FX_LAVA = 0, KRK_FX_SMOKE2D = 1 } krk_fx_mode;

/* One frame's integer inputs. Everything is the fixed-point form of what krk_uniforms carries for the GPU tier. */
typedef struct krk_fx_in {
    int32_t mode;
    int32_t bx, by, brad;   /* brush position and radius, 8.8 cells */
    int32_t bvx, bvy;       /* brush velocity, 8.8 cells per frame */
    int32_t brush_on;
    int32_t emis, heat;     /* smoke / heat added at the brush centre per frame, 0.16 */
    int32_t push;           /* radial push, signed (kick pump) */
    int32_t lift, turb, conf; /* buoyancy, noise force, vorticity confinement */
    int32_t dec_smoke, dec_heat, dec_vel; /* per-frame survival factors, 0.16 */
    int32_t jacobi;
    int32_t nparticles, life_frames, show_particles, show_smoke;
    int32_t spawn_x, spawn_y; /* particle spawn centre, 16.16 pixels */
    int32_t smoke_gain, light_gain, shadow_k, bloom_gain; /* 8.8 */
    int32_t ldx, ldy;       /* light direction as seen from the cells: -1, 0, 1 */
    int32_t noise_t;        /* noise scroll, frames */
    uint32_t frame;
} krk_fx_in;

typedef struct krk_particle {
    int32_t x, y;  /* 16.16 pixels */
    uint16_t age;  /* 8.8 s */
    uint8_t hue;
    uint8_t alive;
} krk_particle;

typedef struct krk_fx {
    int16_t u[KRK_FX_CELLS], v[KRK_FX_CELLS];
    int16_t p[KRK_FX_CELLS], div[KRK_FX_CELLS], curl[KRK_FX_CELLS];
    uint16_t heat[KRK_FX_CELLS], smoke[KRK_FX_CELLS];
    int16_t t0[KRK_FX_CELLS], t1[KRK_FX_CELLS];  /* scratch */
    uint16_t dens[KRK_FX_DW * KRK_FX_DH], dtmp[KRK_FX_DW * KRK_FX_DH];
    uint8_t light[KRK_FX_DW * KRK_FX_DH];
    uint8_t accum[KRK_FX_OW * KRK_FX_OH];
    uint8_t bloom[KRK_FX_CELLS];
    int8_t noise[2][KRK_FX_NOISE * KRK_FX_NOISE]; /* curl-noise vector table, scrolled by time (design 3.6) */
    krk_particle part[KRK_FX_MAXP];
    uint32_t cursor, rng;
    /* sentinels (design 5) */
    uint32_t frames, jacobi_iters, particles_plotted;
} krk_fx;

void krk_fx_init(krk_fx* s);

/* The reference grid the brush and mapper are evaluated on. */
krk_grid krk_fx_grid(void);

/* Frame inputs from the shared core: knobs p, mapper state a (after krk_audio_step), time t, step dt. Brush from
 * krk_brush_at on the 40x32 grid. */
void krk_fx_inputs(krk_fx_in* in, krk_fx_mode mode, const krk_params* p, const krk_audio* a, float t, float dt,
                   uint32_t frame);

/* One simulation step (splat, forces, vorticity, pressure with in.jacobi iterations, advection, particles). */
void krk_fx_step(krk_fx* s, const krk_fx_in* in);

/* Density, blur, light sweep, bloom, composite to palette indices; out holds KRK_FX_OW * KRK_FX_OH bytes, row 0 on top. */
void krk_fx_render(krk_fx* s, const krk_fx_in* in, uint8_t* out);

/* Bakes the 256-colour ramp for the palette knob; rgb[i] = 0xRRGGBB. Uses krk_palette_get and krk_rgb_hex. */
void krk_fx_palette(krk_fx_mode mode, const krk_params* p, uint32_t rgb[256]);

#endif /* KRK_LAVA_FX_H */
