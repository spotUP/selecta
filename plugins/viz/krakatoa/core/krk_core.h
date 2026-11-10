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
    /* Lava-only knobs (design 1.4): stored and clamped here; Lava itself is KRK-P0-4, so they move no Smoke output yet. */
    KRK_K_SMOKE,
    KRK_K_PARTICLES,
    KRK_K_SHOWSMOKE,
    KRK_K_SHOWPARTICLES,
    /* I1246 music reactions, 0 = off, 1 = default strength, up to 3. */
    KRK_K_KICKBURST, /* kick: smoke burst (emission and heat) */
    KRK_K_KICKPUMP,  /* kick: speaker-cone pressure pulse from the emitter (push out, then relax) */
    KRK_K_SNAREFLASH, /* snare: light and bloom flash */
    KRK_K_BASSDRIVE, /* bass level: base flow force and emission */
    KRK_K_HIGHSWIRL, /* highs: swirl and turbulence */
    /* I1052d palette transitions (design 1.3): auto cycle on/off, crossfade length, minimum gap between auto cues. */
    KRK_K_AUTOPALETTE, /* 1: a phrase change moves to the next palette (pref ui.krakatoa.autoPalette) */
    KRK_K_FADE,        /* crossfade seconds; 0 switches at once */
    KRK_K_PALINTERVAL, /* seconds between two auto cues at least */
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
/* Applies a per-effect pref string "name=value,name=value" (the pref ui.krakatoa.smoke; it replaces the reference's
 * localStorage FX_KEYS). Unknown names and bad numbers are skipped. Returns the number of knobs set. */
uint32_t krk_params_parse(krk_params* p, const char* list);

/* Pref keys (include/selecta/prefs.def). */
#define KRK_PREF_PALETTE "ui.krakatoa.palette"
#define KRK_PREF_SMOKE "ui.krakatoa.smoke"
#define KRK_PREF_AUTOPALETTE "ui.krakatoa.autoPalette"

/* ---- Palettes (design 1.3): Aurora, Nebula, Magma, Spectrum, Ash. Linear RGB 0..1. ---- */
typedef struct krk_palette {
    const char* name;
    float hot[4][3];
    float cool[3];
    float key[3];
    float bg[2][3];
    float ember[3]; /* Lava: hottest particle core (KRK-P0-4) */
    float smoke[3];
    float tail[3]; /* Lava: particle trail (KRK-P0-4) */
} krk_palette;

#define KRK_PALETTES 5u
const krk_palette* krk_palette_get(uint32_t index);
/* 0xRRGGBB of a linear colour after the sRGB curve, for --stats and tests. */
uint32_t krk_rgb_hex(const float c[3]);

/* Linear blend of two palettes: every colour of `out` is a + (b - a) * t (t clamped to 0..1; out->name is b's at t > 0.5,
 * else a's). `out` may not alias a or b. */
void krk_palette_blend(krk_palette* out, const krk_palette* a, const krk_palette* b, float t);

typedef struct krk_audio krk_audio;

/* ---- Palette transitions (I1052d, design 1.3): the displayed palette moves from `from` to `to` over KRK_K_FADE seconds.
 * The fire ramp (hot, ember, tail: what fresh emission is drawn with) leads the blend and the smoke body (cool, key, bg,
 * smoke: what old, cooled smoke is drawn with) lags it, so new emission shows the new colours first and old smoke
 * drifts out in the old. Selected by keys 1-5 or by the auto cue. ---- */
typedef struct krk_palfx {
    uint32_t from, to;
    float t;         /* 0..1 along the crossfade; 1 = settled on `to` */
    float since_cue; /* seconds since the last auto cue */
    float age;       /* seconds stepped (no energy cue in the first 2 s) */
    float e_slow;    /* slow energy average for the jump cue */
    uint32_t order, order_seen; /* last tracker order, 1 once seen */
    uint32_t changes; /* palette changes started (sentinel) */
    uint32_t cues;    /* auto cues fired (sentinel) */
} krk_palfx;

void krk_palfx_init(krk_palfx* fx, uint32_t palette, const krk_params* p);
/* Starts a crossfade to palette `index` (0..4) from what is displayed now; the same palette as the target is a no-op.
 * Returns 1 when a change started. Fade 0 settles at once. */
int krk_palfx_select(krk_palfx* fx, uint32_t index, const krk_params* p);
/* One step of dt seconds: advances the crossfade and, with autoPalette on, fires the auto cue: a tracker order change
 * (pos non-NULL: the engine play position, a pattern boundary) or, without a position, an energy jump of the mapper's
 * envelope; at most one per KRK_K_PALINTERVAL seconds. Returns 1 when the cue fired this step. */
int krk_palfx_step(krk_palfx* fx, const krk_params* p, const krk_audio* a, const uint32_t* order, float dt);
/* Fire ramp and smoke body palettes at the current blend. */
void krk_palfx_colors(const krk_palfx* fx, krk_palette* fire, krk_palette* body);

/* ---- Audio mapper (design section 4). ---- */
struct krk_audio {
    /* state */
    float env;      /* RMS envelope, attack 10 ms, release 300 ms */
    float peak;     /* peak follower, decays over 4 s */
    float bpk, mpk, hpk; /* per-band peak followers: instant attack, 4 s decay, floored (I1052x) */
    float bass_avg; /* 0.5 s average of the raw bass band (onset reference) */
    float mid_avg;  /* 0.5 s average of the raw mid band */
    float b_prev, m_prev; /* raw bands of the previous step */
    float since_beat;
    float beat_s;   /* strength of the last kick */
    float since_snare;
    float snare_s;  /* strength of the last snare */
    uint32_t beats; /* kicks detected (sentinel) */
    uint32_t snares; /* snares detected (sentinel) */
    /* outputs, the inputs of one smoke step */
    float L, B, M, H; /* level, bass, mid (snare band), highs: 0..1 after normalisation */
    float force, swirl, emission;
    float heat_mul;  /* temperature injection multiplier after a kick */
    float pump;      /* speaker cone: > 0 push out from the emitter, < 0 relax back; 0 at rest */
    float flash;     /* snare light flash 0..1 */
    float turb_mul;  /* highs on turbulence */
};

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

/* Overrides the colour fields of `u` (built by krk_uniforms_build) with the crossfading palette. */
void krk_uniforms_palette(krk_uniforms* u, const krk_palfx* fx);

#endif /* KRK_CORE_H */
