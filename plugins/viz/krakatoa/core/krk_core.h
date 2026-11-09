/* Krakatoa core (I559 / I1052, design thoughts/shared/research/2026-10-27-krakatoa-port-design.md sections 1.2, 1.4, 4).
 * Portable C99: no OS or SDL headers. Holds what every runtime tier shares for the Smoke mode: the quality table and the
 * grid rule, the knob table (defaults, ranges, clamping), the audio mapper that turns the engine's viz frame into brush
 * force, swirl, emission and beat heat, and the autopilot ("Music brush") path. The GPU tier (sdl/) only consumes this. */
#ifndef KRK_CORE_H
#define KRK_CORE_H

#include <stdint.h>

#include <selecta/viz.h>

/* ---- Quality (design 1.2): Y = g, Z = Y * 0.67, Jacobi iterations per level. ---- */
typedef enum krk_quality { KRK_Q_LOW = 0, KRK_Q_MEDIUM = 1, KRK_Q_HIGH = 2, KRK_Q_ULTRA = 3, KRK_Q_COUNT = 4 } krk_quality;

#define KRK_Q_DEFAULT KRK_Q_HIGH

typedef struct krk_grid {
    uint32_t x, y, z;
    uint32_t jacobi; /* pressure iterations per frame */
} krk_grid;

/* Grid for quality q on a view of w x h pixels: Y = g[q], X = Y * w / h and Z = Y * 0.67, both rounded to the nearest
 * multiple of 4. Matches the four README grids at 1920x1080 (100x56x36, 136x76x52, 200x112x76, 228x128x84). */
krk_grid krk_grid_for(krk_quality q, uint32_t w, uint32_t h);
/* "low", "medium", "high", "ultra"; anything else gives KRK_Q_DEFAULT. */
krk_quality krk_quality_parse(const char* name);
const char* krk_quality_name(krk_quality q);

/* ---- Knobs (design 1.4, Smoke column). ---- */
typedef enum krk_knob {
    KRK_K_BRUSH = 0,
    KRK_K_EMISSION,
    KRK_K_FORCE,
    KRK_K_SWIRL,
    KRK_K_TURBULENCE,
    KRK_K_LIFE,
    KRK_K_LIGHT,
    KRK_K_SHADOWS,
    KRK_K_BLOOM,
    KRK_K_COOLTIME,
    KRK_K_LIFT,
    KRK_K_PALETTE,
    KRK_K_CURSORLIGHT,
    KRK_K_AUTO,
    KRK_K_PAUSED,
    KRK_K_COUNT
} krk_knob;

typedef struct krk_knob_info {
    const char* name; /* the suffix of the pref key ui.krakatoa.smoke.<name> */
    float def, lo, hi;
} krk_knob_info;

const krk_knob_info* krk_knob_table(void);
/* Index of a knob by name, -1 if unknown. */
int krk_knob_find(const char* name);

typedef struct krk_params {
    float v[KRK_K_COUNT];
} krk_params;

void krk_params_default(krk_params* p);
/* Sets knob k to value clamped to its range (palette rounds to 0..4, booleans to 0/1). */
void krk_params_set(krk_params* p, krk_knob k, float value);

/* ---- Palettes (design 1.3): Aurora, Nebula, Magma, Spectrum, Ash. Linear RGB 0..1. ---- */
typedef struct krk_palette {
    const char* name;
    float hot[4][3];
    float cool[3];
    float key[3];
    float bg[2][3];
    float smoke[3];
} krk_palette;

#define KRK_PALETTES 5u
const krk_palette* krk_palette_get(uint32_t index);

/* ---- Audio mapper (design section 4). ---- */
typedef struct krk_audio {
    /* state */
    float env;      /* RMS envelope, attack 10 ms, release 300 ms */
    float peak;     /* peak follower, decays over 4 s */
    float bass_avg; /* 1 s average of B */
    float since_beat;
    float beat_s;   /* strength of the last beat */
    uint32_t beats; /* count of beats detected (sentinel) */
    /* outputs, the inputs of one smoke step */
    float L, B, H;
    float force, swirl, emission;
    float heat_mul; /* temperature injection multiplier: 1 + 2 s for 120 ms after a beat */
} krk_audio;

void krk_audio_init(krk_audio* a);
/* One mapper step of dt seconds for frame f (NULL or seq 0: silence) under knobs p. */
void krk_audio_step(krk_audio* a, const sel_viz_frame* f, const krk_params* p, float dt);

/* ---- Autopilot brush (design 1.4 `auto`: Smoke lower-screen wander on 5.6 of 7 s). ---- */
typedef struct krk_brush {
    float pos[3]; /* grid units */
    float vel[3]; /* grid units per second, clamped to 1.5 Y */
    float radius; /* grid units */
    int active;   /* 1 while the brush emits */
} krk_brush;

/* Brush at time t (seconds) on grid g under knobs p and the mapper's force. With `auto` off the brush stays idle at the
 * bottom centre and only the idle jet (0.15 Y force) drives it. */
void krk_brush_at(krk_brush* b, float t, const krk_grid* g, const krk_params* p, const krk_audio* a);

/* ---- One smoke step's shader inputs. Mirrors the std140 block KrkU in shaders/krk_smoke.comp field for field (all
 * vec4, so the C layout is the std140 layout). Every Smoke knob except `auto` and `paused` lands here; `auto` moves the
 * brush (bp, bv) and `paused` makes the plugin skip the step. ---- */
typedef struct krk_uniforms {
    int32_t dim[4];  /* X, Y, Z, Jacobi iteration index (set per dispatch) */
    float tm[4];     /* dt, time, 1/Y, 0 */
    float bp[4];     /* brush position, radius */
    float bv[4];     /* brush velocity, active */
    float rates[4];  /* uDRate, uTRate x beat heat, uDMax, uKin */
    float forces[4]; /* uEps, uBuoy, uTurb, uPush */
    float decay[4];  /* density, temperature, velocity, freshness */
    float lightp[4]; /* light position, intensity */
    float rend[4];   /* V_EXT, uExtL, uEmis, uBloomInt */
    float view[4];   /* output w, h, V_CAM, frame */
    float hot[4][4];
    float cool[4];
    float keyc[4];
    float bg[2][4];
    float smoke[4];
} krk_uniforms;

/* Fixed reference constants (design 1.4 "fixed"): extinction, camera distance, emission gain, key light direction. */
#define KRK_V_EXT 9.0f
#define KRK_V_CAM 2.4f
#define KRK_EMIS 2.5f

void krk_uniforms_build(krk_uniforms* u, const krk_grid* g, const krk_params* p, const krk_audio* a, const krk_brush* b,
                        float dt, float t, uint32_t out_w, uint32_t out_h, uint32_t frame);

#endif /* KRK_CORE_H */
