/* Krakatoa core: quality table, grid rule, knobs, palettes, audio mapper, autopilot. See krk_core.h. */
#include "krk_core.h"

#include <math.h>
#include <stdlib.h>
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
    {"smoke", 0.55f, 0.0f, 1.5f},      {"particles", 1.0f, 0.0f, 3.0f}, {"showSmoke", 1.0f, 0.0f, 1.0f},
    {"showParticles", 1.0f, 0.0f, 1.0f}, {"kickBurst", 1.0f, 0.0f, 3.0f}, {"kickPump", 1.0f, 0.0f, 3.0f},
    {"snareFlash", 1.0f, 0.0f, 3.0f},  {"bassDrive", 1.0f, 0.0f, 3.0f}, {"highSwirl", 1.0f, 0.0f, 3.0f},
    {"autoPalette", 1.0f, 0.0f, 1.0f}, {"fade", 3.0f, 0.0f, 10.0f},  {"paletteInterval", 30.0f, 5.0f, 300.0f},
};

static int knob_is_bool(krk_knob k) {
    return k == KRK_K_CURSORLIGHT || k == KRK_K_AUTO || k == KRK_K_PAUSED || k == KRK_K_SHOWSMOKE ||
           k == KRK_K_SHOWPARTICLES || k == KRK_K_AUTOPALETTE;
}

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
    } else if (knob_is_bool(k)) {
        value = value >= 0.5f ? 1.0f : 0.0f;
    }
    p->v[k] = value;
}

uint32_t krk_params_parse(krk_params* p, const char* list) {
    char name[32];
    uint32_t set = 0u;
    while (list != NULL && *list != '\0') {
        const char* eq = strchr(list, '=');
        const char* end = strchr(list, ',');
        size_t n;
        if (end == NULL) {
            end = list + strlen(list);
        }
        if (eq != NULL && eq < end && (n = (size_t)(eq - list)) > 0u && n < sizeof name) {
            char* stop;
            float v;
            int k;
            memcpy(name, list, n);
            name[n] = '\0';
            v = strtof(eq + 1, &stop);
            k = krk_knob_find(name);
            if (k >= 0 && stop != eq + 1 && stop == end) {
                krk_params_set(p, (krk_knob)k, v);
                set++;
            }
        }
        list = *end == ',' ? end + 1 : end;
    }
    return set;
}

/* ---- Palettes (design 1.3): reference index.html PALETTES (lines 275-281), hex converted with the reference toLin
 * (line 1672: (c/255)^2.2). ---- */
static const krk_palette k_pal[KRK_PALETTES] = {
    {"Aurora", {{0.6800f, 1.0000f, 0.0415f}, {0.0718f, 1.0000f, 0.3587f}, {0.0135f, 0.7818f, 1.0000f}, {0.3392f, 1.0000f, 0.0658f}},
     {0.0066f, 0.0399f, 1.0000f}, {0.0210f, 0.0697f, 1.0000f}, {{0.0000f, 0.0000f, 0.0004f}, {0.0001f, 0.0023f, 0.0152f}}, {0.1975f, 1.0000f, 0.0254f},
     {0.0242f, 0.1062f, 1.0000f}, {0.0189f, 0.0254f, 0.7519f}},
    {"Nebula", {{1.0000f, 0.0242f, 0.3686f}, {1.0000f, 0.0718f, 0.1542f}, {0.5605f, 0.0415f, 1.0000f}, {1.0000f, 0.1604f, 0.6800f}},
     {0.0078f, 0.2508f, 1.0000f}, {0.1011f, 0.0210f, 1.0000f}, {{0.0001f, 0.0000f, 0.0004f}, {0.0037f, 0.0003f, 0.0231f}}, {0.4423f, 0.0415f, 1.0000f},
     {0.0385f, 0.2674f, 1.0000f}, {0.0385f, 0.0170f, 0.4878f}},
    {"Magma", {{1.0000f, 0.8751f, 0.3297f}, {1.0000f, 0.4423f, 0.0415f}, {1.0000f, 0.1304f, 0.0078f}, {1.0000f, 0.6524f, 0.1011f}},
     {0.2047f, 0.0017f, 0.0090f}, {1.0000f, 0.0658f, 0.0135f}, {{0.0001f, 0.0000f, 0.0000f}, {0.0066f, 0.0003f, 0.0002f}}, {1.0000f, 0.0619f, 0.0023f},
     {0.2590f, 0.2716f, 0.3021f}, {0.0329f, 0.0342f, 0.0415f}},
    {"Spectrum", {{1.0000f, 0.0399f, 0.1062f}, {1.0000f, 0.6592f, 0.0399f}, {0.0399f, 1.0000f, 0.4535f}, {0.0399f, 0.2674f, 1.0000f}},
     {0.0658f, 0.0097f, 0.4423f}, {0.1036f, 0.0415f, 1.0000f}, {{0.0001f, 0.0001f, 0.0005f}, {0.0012f, 0.0008f, 0.0119f}}, {1.0000f, 0.0399f, 0.6388f},
     {0.1636f, 0.0781f, 0.7519f}, {0.0254f, 0.0055f, 0.2590f}},
    {"Ash", {{1.0000f, 1.0000f, 1.0000f}, {1.0000f, 0.8122f, 0.6320f}, {0.7818f, 0.8592f, 1.0000f}, {1.0000f, 0.9075f, 0.7818f}},
     {0.0915f, 0.1193f, 0.1835f}, {0.2195f, 0.2845f, 0.4647f}, {{0.0001f, 0.0001f, 0.0001f}, {0.0020f, 0.0026f, 0.0050f}}, {1.0000f, 0.1975f, 0.0210f},
     {0.3787f, 0.4205f, 0.5114f}, {0.0582f, 0.0677f, 0.0963f}},
};

const krk_palette* krk_palette_get(uint32_t index) {
    return &k_pal[index < KRK_PALETTES ? index : 0u];
}

static uint32_t srgb8(float v) {
    v = v <= 0.0f ? 0.0f : v >= 1.0f ? 1.0f : v;
    v = v <= 0.0031308f ? 12.92f * v : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
    return (uint32_t)(v * 255.0f + 0.5f);
}

uint32_t krk_rgb_hex(const float c[3]) {
    return (srgb8(c[0]) << 16) | (srgb8(c[1]) << 8) | srgb8(c[2]);
}

/* ---- Audio mapper (design section 4) ---- */
void krk_audio_init(krk_audio* a) {
    memset(a, 0, sizeof *a);
    a->peak = 1e-3f;
    a->since_beat = 10.0f;
    a->heat_mul = 1.0f;
    a->turb_mul = 1.0f;
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

/* Per-band normalisation (I1052x): a peak follower with instant attack and a 4 s decay, divided out with a floor so
 * quiet passages and silence do not blow noise up to full scale. */
#define KRK_BAND_FLOOR 0.04f

static float band_norm(float raw, float* pk, float dt) {
    *pk *= expf(-dt / 4.0f);
    if (raw > *pk) {
        *pk = raw;
    }
    raw /= *pk > KRK_BAND_FLOOR ? *pk : KRK_BAND_FLOOR;
    return raw > 1.0f ? 1.0f : raw;
}

static float clamp01(float v) {
    return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

/* I1246: the reaction lands on the frame the spectrum shows the hit. Onsets fire on the first frame a band rises above
 * its 0.5 s average (no smoothing on attack); every envelope starts at full strength on that frame and only the release
 * is shaped. Kick (bands 0-3, 43-140 Hz) = smoke burst + speaker-cone pump; snare (bands 8-11, 1-3.2 kHz, a separate
 * detector) = light flash; bass level = base flow; highs = swirl and turbulence. */
void krk_audio_step(krk_audio* a, const sel_viz_frame* f, const krk_params* p, float dt) {
    int live = f != NULL && f->seq != 0u;
    int spec = live && f->spectrum_valid;
    float rms = live ? frame_rms(f) : 0.0f;
    float k = rms > a->env ? 1.0f - expf(-dt / 0.010f) : 1.0f - expf(-dt / 0.300f);
    float avg_k = 1.0f - expf(-dt / 0.5f);
    float kb = p->v[KRK_K_KICKBURST], kp = p->v[KRK_K_KICKPUMP], sf = p->v[KRK_K_SNAREFLASH];
    float bd = p->v[KRK_K_BASSDRIVE], hs = p->v[KRK_K_HIGHSWIRL];
    float t, burst, br, mr, rb, rm;
    a->env += (rms - a->env) * k;
    a->peak *= expf(-dt / 4.0f);
    if (a->env > a->peak) {
        a->peak = a->env;
    }
    a->L = a->peak > 1e-4f ? a->env / a->peak : 0.0f;
    br = spec ? band_mean(f, 0u, 3u) : 0.0f;
    mr = spec ? band_mean(f, 8u, 11u) : 0.0f;
    a->B = band_norm(br, &a->bpk, dt);
    a->M = band_norm(mr, &a->mpk, dt);
    a->H = spec ? band_norm(band_mean(f, 12u, 15u), &a->hpk, dt) : 0.0f;
    a->since_beat += dt;
    a->since_snare += dt;
    /* Onsets run on the raw band against its own 0.5 s average (scale free, so the peak normalisation cannot flatten
     * the hit). Rise = jump over the previous frame relative to the average. */
    rb = (br - a->b_prev) / (a->bass_avg > KRK_BAND_FLOOR ? a->bass_avg : KRK_BAND_FLOOR);
    rm = (mr - a->m_prev) / (a->mid_avg > KRK_BAND_FLOOR ? a->mid_avg : KRK_BAND_FLOOR);
    /* kick: bass rises, sits 1.3 x above its average and above 0.35 of its peak; 150 ms refractory */
    if (rb > 0.0f && br > 1.3f * a->bass_avg && a->B > 0.35f && br > 0.5f * KRK_BAND_FLOOR && a->since_beat >= 0.150f) {
        a->beat_s = clamp01((br - a->bass_avg) / (a->bass_avg > KRK_BAND_FLOOR ? a->bass_avg : KRK_BAND_FLOOR));
        a->since_beat = 0.0f;
        a->beats++;
    }
    /* snare: the same rule on the mid band, separate from the kick: the mid rise must beat the bass rise, so a kick's
     * overtones in the mids do not flash */
    if (rm > 0.0f && rm > rb && mr > 1.3f * a->mid_avg && a->M > 0.35f && mr > 0.5f * KRK_BAND_FLOOR &&
        a->since_snare >= 0.120f) {
        a->snare_s = clamp01((mr - a->mid_avg) / (a->mid_avg > KRK_BAND_FLOOR ? a->mid_avg : KRK_BAND_FLOOR));
        a->since_snare = 0.0f;
        a->snares++;
    }
    a->bass_avg += (br - a->bass_avg) * avg_k;
    a->mid_avg += (mr - a->mid_avg) * avg_k;
    a->b_prev = br;
    a->m_prev = mr;
    t = a->since_beat;
    burst = a->beat_s * expf(-t / 0.12f);
    /* speaker cone: out on the hit (full on frame 0), then pulled back below rest, settling over ~0.5 s */
    a->pump = kp * a->beat_s * (expf(-t / 0.09f) - 0.45f * (expf(-t / 0.30f) - expf(-t / 0.06f)));
    a->force = p->v[KRK_K_FORCE] * (0.4f + 0.9f * bd * a->B + 0.3f * a->L);
    a->swirl = p->v[KRK_K_SWIRL] * (0.5f + 1.2f * hs * a->H);
    a->turb_mul = 1.0f + hs * a->H;
    a->emission = p->v[KRK_K_EMISSION] * (0.3f + 1.2f * bd * a->B) + 4.0f * kb * burst;
    a->heat_mul = t < 0.120f ? 1.0f + 2.0f * kb * a->beat_s : 1.0f;
    a->flash = sf * a->snare_s * expf(-a->since_snare / 0.09f);
}

/* ---- Autopilot ---- */
void krk_brush_at(krk_brush* b, float t, const krk_grid* g, const krk_params* p, const krk_audio* a) {
    float Y = (float)g->y, X = (float)g->x, Z = (float)g->z;
    float force = a != NULL ? a->force : p->v[KRK_K_FORCE];
    float kv = 0.7f * force * Y, cap = 1.5f * Y, sp;
    uint32_t i;
    /* the emitter breathes with the kick pump (I1246) */
    b->radius = p->v[KRK_K_BRUSH] * Y * 4.0f * (1.0f + 0.5f * (a != NULL ? a->pump : 0.0f));
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
    u->forces[2] = 0.3f * p->v[KRK_K_TURBULENCE] * a->turb_mul * Y;
    /* uPush is the radial push out of the emitter (krk_smoke.comp); the kick pump drives it out and back (I1246) */
    u->forces[3] = 0.5f * a->force * Y * (1.0f + 4.0f * a->pump);
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
    u->lightp[3] = 1.6f * p->v[KRK_K_LIGHT] * (1.0f + 2.5f * a->flash);
    u->rend[0] = KRK_V_EXT;
    u->rend[1] = 27.0f * p->v[KRK_K_SHADOWS];
    u->rend[2] = KRK_EMIS;
    u->rend[3] = 0.16f * p->v[KRK_K_BLOOM] * (1.0f + 3.0f * a->flash);
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

/* ---- Palette transitions (I1052d) ---- */
static float lerpf(float a, float b, float t) {
    return a * (1.0f - t) + b * t; /* exact at t = 0 and 1 */
}

static void blend3(float out[3], const float a[3], const float b[3], float t) {
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        out[i] = lerpf(a[i], b[i], t);
    }
}

void krk_palette_blend(krk_palette* out, const krk_palette* a, const krk_palette* b, float t) {
    uint32_t i;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    out->name = t > 0.5f ? b->name : a->name;
    for (i = 0; i < 4u; i++) {
        blend3(out->hot[i], a->hot[i], b->hot[i], t);
    }
    blend3(out->cool, a->cool, b->cool, t);
    blend3(out->key, a->key, b->key, t);
    blend3(out->bg[0], a->bg[0], b->bg[0], t);
    blend3(out->bg[1], a->bg[1], b->bg[1], t);
    blend3(out->ember, a->ember, b->ember, t);
    blend3(out->smoke, a->smoke, b->smoke, t);
    blend3(out->tail, a->tail, b->tail, t);
}

void krk_palfx_init(krk_palfx* fx, uint32_t palette, const krk_params* p) {
    memset(fx, 0, sizeof *fx);
    fx->from = fx->to = palette < KRK_PALETTES ? palette : 0u;
    fx->t = 1.0f;
    fx->since_cue = p->v[KRK_K_PALINTERVAL]; /* the first phrase change may cue at once */
}

int krk_palfx_select(krk_palfx* fx, uint32_t index, const krk_params* p) {
    if (index >= KRK_PALETTES || index == fx->to) { /* unknown, or settled on / already heading to it */
        return 0;
    }
    /* a change in mid-fade restarts from the side that is more visible now */
    fx->from = fx->t >= 0.5f ? fx->to : fx->from;
    fx->to = index;
    fx->t = p->v[KRK_K_FADE] <= 0.0f ? 1.0f : 0.0f;
    if (fx->t >= 1.0f) {
        fx->from = index;
    }
    fx->changes++;
    return 1;
}

int krk_palfx_step(krk_palfx* fx, const krk_params* p, const krk_audio* a, const uint32_t* order, float dt) {
    int cue = 0;
    float env = a != NULL ? a->env : 0.0f;
    if (fx->age <= 0.0f) {
        fx->e_slow = env; /* the average starts at the first level, so a song start is no jump */
    }
    fx->age += dt;
    if (fx->t < 1.0f) {
        fx->t += p->v[KRK_K_FADE] > 0.0f ? dt / p->v[KRK_K_FADE] : 1.0f;
        if (fx->t >= 1.0f) {
            fx->t = 1.0f;
            fx->from = fx->to;
        }
    }
    fx->since_cue += dt;
    if (order != NULL) {
        cue = fx->order_seen != 0u && *order != fx->order; /* a pattern boundary */
        fx->order = *order;
        fx->order_seen = 1u;
    } else {
        cue = fx->age > 2.0f && fx->e_slow > 0.02f && env > fx->e_slow * 1.8f + 0.05f; /* a big energy jump */
    }
    fx->e_slow += (env - fx->e_slow) * (dt < 4.0f ? dt / 4.0f : 1.0f);
    if (cue && p->v[KRK_K_AUTOPALETTE] >= 0.5f && fx->since_cue >= p->v[KRK_K_PALINTERVAL]) {
        fx->since_cue = 0.0f;
        fx->cues++;
        krk_palfx_select(fx, (fx->to + 1u) % KRK_PALETTES, p);
        return 1;
    }
    return 0;
}

void krk_palfx_colors(const krk_palfx* fx, krk_palette* fire, krk_palette* body) {
    const krk_palette* a = krk_palette_get(fx->from);
    const krk_palette* b = krk_palette_get(fx->to);
    float lead = 1.0f - (1.0f - fx->t) * (1.0f - fx->t), lag = fx->t * fx->t;
    krk_palette_blend(fire, a, b, lead);
    krk_palette_blend(body, a, b, lag);
}

void krk_uniforms_palette(krk_uniforms* u, const krk_palfx* fx) {
    krk_palette fire, body;
    uint32_t i;
    krk_palfx_colors(fx, &fire, &body);
    for (i = 0; i < 4u; i++) {
        set3(u->hot[i], fire.hot[i], 1.0f);
    }
    set3(u->cool, body.cool, 1.0f);
    set3(u->keyc, body.key, 1.0f);
    set3(u->bg[0], body.bg[0], 1.0f);
    set3(u->bg[1], body.bg[1], 1.0f);
    set3(u->smoke, body.smoke, 1.0f);
}
