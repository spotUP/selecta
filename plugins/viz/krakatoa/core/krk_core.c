/* Krakatoa core: quality table, grid rule, knobs, palettes, audio mapper, autopilot. See krk_core.h. */
#include "krk_core.h"

#include <math.h>
#include <string.h>

/* ---- Quality ---- */
static const uint32_t k_g[KRK_Q_COUNT] = {56u, 76u, 112u, 128u};
static const uint32_t k_jac[KRK_Q_COUNT] = {16u, 20u, 28u, 30u};
static const char* const k_qname[KRK_Q_COUNT] = {"low", "medium", "high", "ultra"};

krk_grid krk_grid_for(krk_quality q, uint32_t w, uint32_t h) {
    krk_grid g;
    uint32_t y;
    if ((unsigned)q >= KRK_Q_COUNT) {
        q = KRK_Q_DEFAULT;
    }
    if (w == 0u || h == 0u) {
        w = 16u;
        h = 9u;
    }
    y = k_g[q];
    g.y = y;
    /* round(Y * w / h / 4) * 4 and round(Y * 0.67 / 4) * 4, in integers */
    g.x = (uint32_t)(((uint64_t)y * w + 2u * (uint64_t)h) / (4u * (uint64_t)h)) * 4u;
    g.z = ((y * 67u + 200u) / 400u) * 4u;
    if (g.x < 4u) {
        g.x = 4u;
    }
    /* Host surfaces can be far wider than a screen (the Now Playing strip). The reference sizes the grid from a
     * window; cap X at 3 Y so a strip does not allocate a 7M-cell grid. Screen aspects (up to 21:9) never hit it. */
    if (g.x > 3u * y) {
        g.x = 3u * y;
    }
    g.jacobi = k_jac[q];
    return g;
}

krk_quality krk_quality_parse(const char* name) {
    uint32_t i;
    for (i = 0; name != NULL && i < KRK_Q_COUNT; i++) {
        if (strcmp(name, k_qname[i]) == 0) {
            return (krk_quality)i;
        }
    }
    return KRK_Q_DEFAULT;
}

const char* krk_quality_name(krk_quality q) {
    return (unsigned)q < KRK_Q_COUNT ? k_qname[q] : k_qname[KRK_Q_DEFAULT];
}

/* ---- Knobs (design 1.4) ---- */
static const krk_knob_info k_knobs[KRK_K_COUNT] = {
    {"brush", 0.03f, 0.012f, 0.08f},   {"emission", 1.0f, 0.2f, 2.5f}, {"force", 1.0f, 0.2f, 2.5f},
    {"swirl", 26.0f, 0.0f, 60.0f},     {"turbulence", 1.0f, 0.0f, 3.0f}, {"life", 1.0f, 0.3f, 3.0f},
    {"light", 1.0f, 0.0f, 3.0f},       {"shadows", 1.0f, 0.0f, 3.0f},  {"bloom", 0.9f, 0.0f, 2.0f},
    {"coolTime", 0.5f, 0.15f, 2.0f},   {"lift", 1.0f, 0.0f, 3.0f},     {"palette", 0.0f, 0.0f, 4.0f},
    {"cursorLight", 1.0f, 0.0f, 1.0f}, {"auto", 1.0f, 0.0f, 1.0f},     {"paused", 0.0f, 0.0f, 1.0f},
};

const krk_knob_info* krk_knob_table(void) {
    return k_knobs;
}

int krk_knob_find(const char* name) {
    int i;
    for (i = 0; name != NULL && i < (int)KRK_K_COUNT; i++) {
        if (strcmp(name, k_knobs[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

void krk_params_default(krk_params* p) {
    uint32_t i;
    for (i = 0; i < KRK_K_COUNT; i++) {
        p->v[i] = k_knobs[i].def;
    }
}

void krk_params_set(krk_params* p, krk_knob k, float value) {
    const krk_knob_info* in;
    if ((unsigned)k >= KRK_K_COUNT || value != value) {
        return;
    }
    in = &k_knobs[k];
    value = value < in->lo ? in->lo : value > in->hi ? in->hi : value;
    if (k == KRK_K_PALETTE) {
        value = floorf(value + 0.5f);
    } else if (k == KRK_K_CURSORLIGHT || k == KRK_K_AUTO || k == KRK_K_PAUSED) {
        value = value >= 0.5f ? 1.0f : 0.0f;
    }
    p->v[k] = value;
}

/* ---- Palettes (design 1.3). The reference hex values are not available (design 1.3 names the lines but not the values);
 * these ramps follow the names and are to be replaced 1:1 once the reference file is at hand. ---- */
static const krk_palette k_pal[KRK_PALETTES] = {
    {"Aurora", {{0.10f, 0.95f, 0.60f}, {0.20f, 0.70f, 1.00f}, {0.60f, 0.35f, 1.00f}, {0.95f, 0.95f, 1.00f}},
     {0.05f, 0.25f, 0.30f}, {0.85f, 1.00f, 0.95f}, {{0.01f, 0.02f, 0.05f}, {0.02f, 0.06f, 0.10f}}, {0.55f, 0.70f, 0.75f}},
    {"Nebula", {{0.90f, 0.30f, 0.90f}, {0.45f, 0.30f, 1.00f}, {1.00f, 0.55f, 0.75f}, {1.00f, 0.95f, 1.00f}},
     {0.15f, 0.08f, 0.30f}, {1.00f, 0.90f, 1.00f}, {{0.02f, 0.01f, 0.05f}, {0.06f, 0.02f, 0.10f}}, {0.60f, 0.55f, 0.75f}},
    {"Magma", {{1.00f, 0.25f, 0.02f}, {1.00f, 0.55f, 0.05f}, {1.00f, 0.85f, 0.30f}, {1.00f, 1.00f, 0.85f}},
     {0.25f, 0.06f, 0.03f}, {1.00f, 0.92f, 0.80f}, {{0.03f, 0.01f, 0.01f}, {0.08f, 0.03f, 0.02f}}, {0.45f, 0.40f, 0.38f}},
    {"Spectrum", {{1.00f, 0.15f, 0.20f}, {1.00f, 0.85f, 0.10f}, {0.10f, 0.90f, 0.40f}, {0.20f, 0.50f, 1.00f}},
     {0.10f, 0.10f, 0.20f}, {1.00f, 1.00f, 1.00f}, {{0.02f, 0.02f, 0.03f}, {0.05f, 0.05f, 0.08f}}, {0.65f, 0.65f, 0.70f}},
    {"Ash", {{0.90f, 0.55f, 0.30f}, {0.80f, 0.75f, 0.70f}, {0.95f, 0.92f, 0.88f}, {1.00f, 1.00f, 1.00f}},
     {0.12f, 0.12f, 0.12f}, {1.00f, 0.97f, 0.92f}, {{0.03f, 0.03f, 0.03f}, {0.08f, 0.08f, 0.08f}}, {0.55f, 0.55f, 0.55f}},
};

const krk_palette* krk_palette_get(uint32_t index) {
    return &k_pal[index < KRK_PALETTES ? index : 0u];
}

/* ---- Audio mapper (design section 4) ---- */
void krk_audio_init(krk_audio* a) {
    memset(a, 0, sizeof *a);
    a->peak = 1e-3f;
    a->since_beat = 10.0f;
    a->heat_mul = 1.0f;
}

static float frame_rms(const sel_viz_frame* f) {
    uint32_t i, n = f->rate / SEL_VIZ_BLOCKS_PER_SECOND;
    double acc = 0.0;
    if (n == 0u || n > SEL_VIZ_PCM_FRAMES) {
        n = SEL_VIZ_PCM_FRAMES;
    }
    for (i = SEL_VIZ_PCM_FRAMES - n; i < SEL_VIZ_PCM_FRAMES; i++) {
        double l = f->pcm[i * 2u] / 32768.0, r = f->pcm[i * 2u + 1u] / 32768.0;
        acc += 0.5 * (l * l + r * r);
    }
    return (float)sqrt(acc / (double)n);
}

static float band_mean(const sel_viz_frame* f, uint32_t from, uint32_t to) {
    uint32_t i;
    float s = 0.0f;
    for (i = from; i <= to; i++) {
        s += f->band[i] < 0 ? 0.0f : (float)f->band[i] / 32767.0f;
    }
    return s / (float)(to - from + 1u);
}

void krk_audio_step(krk_audio* a, const sel_viz_frame* f, const krk_params* p, float dt) {
    int live = f != NULL && f->seq != 0u;
    float rms = live ? frame_rms(f) : 0.0f;
    float k = rms > a->env ? 1.0f - expf(-dt / 0.010f) : 1.0f - expf(-dt / 0.300f);
    float b, s;
    a->env += (rms - a->env) * k;
    a->peak *= expf(-dt / 4.0f);
    if (a->env > a->peak) {
        a->peak = a->env;
    }
    a->L = a->peak > 1e-4f ? a->env / a->peak : 0.0f;
    a->B = live && f->spectrum_valid ? band_mean(f, 0u, 3u) : 0.0f;
    a->H = live && f->spectrum_valid ? band_mean(f, 12u, 15u) : 0.0f;
    b = a->B;
    a->since_beat += dt;
    if (a->bass_avg > 1e-3f && b > 1.4f * a->bass_avg && a->since_beat >= 0.200f) {
        s = (b - a->bass_avg) / a->bass_avg;
        a->beat_s = s < 0.0f ? 0.0f : s > 1.0f ? 1.0f : s;
        a->since_beat = 0.0f;
        a->beats++;
    }
    a->bass_avg += (b - a->bass_avg) * (1.0f - expf(-dt / 1.0f));
    a->force = p->v[KRK_K_FORCE] * (0.5f + a->L);
    a->swirl = p->v[KRK_K_SWIRL] * (0.5f + a->H);
    a->emission = p->v[KRK_K_EMISSION] * (0.3f + 1.2f * a->B) + 3.0f * a->beat_s * expf(-a->since_beat / 0.12f);
    a->heat_mul = a->since_beat < 0.120f ? 1.0f + 2.0f * a->beat_s : 1.0f;
}

/* ---- Autopilot ---- */
void krk_brush_at(krk_brush* b, float t, const krk_grid* g, const krk_params* p, const krk_audio* a) {
    float Y = (float)g->y, X = (float)g->x, Z = (float)g->z;
    float force = a != NULL ? a->force : p->v[KRK_K_FORCE];
    float kv = 0.7f * force * Y, cap = 1.5f * Y, sp;
    uint32_t i;
    b->radius = p->v[KRK_K_BRUSH] * Y * 4.0f;
    b->pos[2] = Z * 0.5f; /* emitters at Z/2 */
    if (p->v[KRK_K_AUTO] >= 0.5f) {
        /* lower-screen wander: on for 5.6 s of every 7 s */
        float ph = fmodf(t, 7.0f);
        float w1 = 0.9f, w2 = 1.7f;
        b->active = ph < 5.6f;
        b->pos[0] = X * (0.5f + 0.32f * sinf(w1 * t));
        b->pos[1] = Y * (0.22f + 0.10f * sinf(w2 * t + 0.7f));
        b->vel[0] = X * 0.32f * w1 * cosf(w1 * t) / X * kv;
        b->vel[1] = Y * 0.10f * w2 * cosf(w2 * t + 0.7f) / Y * kv;
        b->vel[2] = 0.0f;
    } else {
        b->active = 1;
        b->pos[0] = X * 0.5f;
        b->pos[1] = Y * 0.12f;
        b->vel[0] = 0.0f;
        b->vel[1] = 0.0f;
        b->vel[2] = 0.0f;
    }
    b->vel[1] += 0.15f * Y * force; /* idle jet */
    sp = sqrtf(b->vel[0] * b->vel[0] + b->vel[1] * b->vel[1] + b->vel[2] * b->vel[2]);
    if (sp > cap) {
        for (i = 0; i < 3u; i++) {
            b->vel[i] *= cap / sp;
        }
    }
}

/* ---- Shader inputs for one step ---- */
static void set3(float* d, const float* s, float w) {
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
    d[3] = w;
}

void krk_uniforms_build(krk_uniforms* u, const krk_grid* g, const krk_params* p, const krk_audio* a, const krk_brush* b,
                        float dt, float t, uint32_t out_w, uint32_t out_h, uint32_t frame) {
    const krk_palette* pal = krk_palette_get((uint32_t)p->v[KRK_K_PALETTE]);
    float X = (float)g->x, Y = (float)g->y, Z = (float)g->z;
    float life = p->v[KRK_K_LIFE], cool = p->v[KRK_K_COOLTIME];
    uint32_t i;
    memset(u, 0, sizeof *u);
    u->dim[0] = (int32_t)g->x;
    u->dim[1] = (int32_t)g->y;
    u->dim[2] = (int32_t)g->z;
    u->tm[0] = dt;
    u->tm[1] = t;
    u->tm[2] = 1.0f / Y;
    set3(u->bp, b->pos, b->radius);
    set3(u->bv, b->vel, b->active ? 1.0f : 0.0f);
    u->rates[0] = 9.0f * a->emission;
    u->rates[1] = 9.0f * a->heat_mul;
    u->rates[2] = 3.0f;
    u->rates[3] = 0.6f;
    u->forces[0] = a->swirl * 0.002f * Y;
    u->forces[1] = 0.9f * p->v[KRK_K_LIFT] * Y;
    u->forces[2] = 0.3f * p->v[KRK_K_TURBULENCE] * Y;
    u->forces[3] = 0.5f * a->force * Y;
    u->decay[0] = expf(-dt / (2.2f * life));
    u->decay[1] = expf(-dt / cool);
    u->decay[2] = expf(-0.15f * dt);
    u->decay[3] = expf(-dt / (0.92f * life));
    if (p->v[KRK_K_CURSORLIGHT] >= 0.5f) {
        /* the light rides above and in front of the brush */
        u->lightp[0] = b->pos[0];
        u->lightp[1] = b->pos[1] + 0.35f * Y;
        u->lightp[2] = b->pos[2] - 0.25f * Z;
    } else {
        u->lightp[0] = 0.45f * X;
        u->lightp[1] = 0.85f * Y;
        u->lightp[2] = 0.28f * Z;
    }
    u->lightp[3] = 1.6f * p->v[KRK_K_LIGHT];
    u->rend[0] = KRK_V_EXT;
    u->rend[1] = 27.0f * p->v[KRK_K_SHADOWS];
    u->rend[2] = KRK_EMIS;
    u->rend[3] = 0.16f * p->v[KRK_K_BLOOM];
    u->view[0] = (float)out_w;
    u->view[1] = (float)out_h;
    u->view[2] = KRK_V_CAM;
    u->view[3] = (float)(frame & 0xFFFFu);
    for (i = 0; i < 4u; i++) {
        set3(u->hot[i], pal->hot[i], 1.0f);
    }
    set3(u->cool, pal->cool, 1.0f);
    set3(u->keyc, pal->key, 1.0f);
    set3(u->bg[0], pal->bg[0], 1.0f);
    set3(u->bg[1], pal->bg[1], 1.0f);
    set3(u->smoke, pal->smoke, 1.0f);
}
