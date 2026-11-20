/* SID PatternScope viz plugin (I434j2, design 2026-11-01-sid-register-scope.md section 2.5): the HippoPlayer register view.
 * Per SID chip a 40 x 8 cell band: per voice the filter route, frequency and pulse width bars, the eight control flags
 * (with afterglow), a vertical envelope bar; per chip the filter cutoff, resonance and LP/BP/HP row. The raw registers
 * and the glow counters come from the engine (sel_viz_sid_chip); all decoding lives here. With a host pen (I1230, the
 * desktop) the same registers are drawn as app-design cards in the app font; without one (Amiga MUI, unit tests) as the
 * pixel-glyph grid below. */
#include "viz/glyphs.h"

#define COLS 40
#define ROWS 8
#define VOICE_COLS 13 /* voice v starts at column 13 * v */

typedef struct label {
    uint8_t col, row;
    const char* text;
} label;

/* Per voice, relative to the voice column (the heading "Voice<n>" is per chip). Full words (I1466): at the glyph pixel scale
 * sx = cw / 8 a glyph advances 5 * sx = 5/8 of a cell, so "Triangle" (8 glyphs, 39 sx) fills the 5 cells 6..10 and "Pulse Width"
 * (11 glyphs, 54 sx) the 7 cells 0..6 before its bar. labels() drops the words when cw < 8 or the glyph pixel is wider than sx. */
static const label k_voice_labels[] = {
    {7, 0, "Filter"},    {0, 1, "Frequency"}, {0, 2, "Pulse Width"}, {0, 3, "Gate"},     {6, 3, "Triangle"},
    {0, 4, "Sync"},      {6, 4, "Sawtooth"},  {0, 5, "Ring"},        {6, 5, "Pulse"},    {0, 6, "Test"},
    {6, 6, "Noise"},
};
/* Per chip: Filter, cutoff bar (cols 5..9), Resonance (cols 11..16), its bar (17..18), then three mode blocks at 19, 26, 33 each
 * followed by a 6 cell word ("High Pass" is 44 sx wide). */
static const label k_chip_labels[] = {
    {0, 7, "Filter"}, {11, 7, "Resonance"}, {20, 7, "Low Pass"}, {27, 7, "Band Pass"}, {34, 7, "High Pass"},
};
static const char* const k_headings[9] = {"Voice1", "Voice2", "Voice3", "Voice4", "Voice5",
                                          "Voice6", "Voice7", "Voice8", "Voice9"};

/* A control flag cell: control register bit, column and row relative to the voice column. */
typedef struct flag {
    uint8_t bit, col, row;
} flag;
static const flag k_flags[8] = {{0, 5, 3}, {4, 11, 3}, {1, 5, 4}, {5, 11, 4}, {2, 5, 5}, {6, 11, 5}, {3, 5, 6}, {7, 11, 6}};

typedef struct grid {
    sel_surface* s;
    const sel_viz_colors* c;
    int32_t y0, cw, rh;
} grid;

static int32_t cell_x(const grid* g, int32_t col) { return col * g->cw; }
static int32_t cell_y(const grid* g, int32_t row) { return g->y0 + row * g->rh; }

/* Decoding (HippoPlayer patternScopeSIDUpdate, design section 1). */
static uint32_t freq_scaled(const uint8_t* reg) {
    uint32_t f = (uint32_t)reg[0] | ((uint32_t)reg[1] << 8);
    if (f < 0x4000u) {
        return f * 4u;
    }
    return f < 0x8000u ? f * 2u : f;
}
static uint32_t pulse_width(const uint8_t* reg) { return ((uint32_t)reg[2] | ((uint32_t)reg[3] << 8)) & 0xFFFu; }
static uint32_t cutoff(const uint8_t* reg) { return ((uint32_t)reg[0x16] << 3) | ((uint32_t)reg[0x15] & 7u); }

/* Horizontal bar over `cols` cells: track in grid, fill in primary, filled in proportion value / max (the largest value). */
static uint32_t bar(const grid* g, int32_t col, int32_t row, int32_t cols, uint32_t value, uint32_t max) {
    int32_t w = cols * g->cw, fill;
    int32_t y = cell_y(g, row) + (g->rh > 2 ? 1 : 0), h = g->rh > 2 ? g->rh - 2 : g->rh;
    vs_rect(g->s, cell_x(g, col), y, w, h, g->c->grid);
    fill = (int32_t)((uint32_t)w * value / max);
    if (fill > w) {
        fill = w;
    }
    if (fill > 0) {
        vs_rect(g->s, cell_x(g, col), y, fill, h, g->c->primary);
        return 2u;
    }
    return 1u;
}

/* One flag cell: on = primary, glow 4..1 = secondary block shrinking towards the bottom, off = grid outline. */
static uint32_t block(const grid* g, int32_t col, int32_t row, int on, uint32_t glow) {
    int32_t in = (g->cw >= 3 && g->rh >= 3) ? 1 : 0;
    int32_t x = cell_x(g, col) + in, y = cell_y(g, row) + in, w = g->cw - 2 * in, h = g->rh - 2 * in;
    if (on) {
        vs_rect(g->s, x, y, w, h, g->c->primary);
        return 1u;
    }
    if (glow > 0u) {
        int32_t gh = g->rh * (int32_t)glow / 5;
        gh = gh < 1 ? 1 : gh;
        vs_rect(g->s, x, cell_y(g, row) + g->rh - in - gh, w, gh, g->c->secondary);
        return 1u;
    }
    vs_rect(g->s, x, y, w, h, g->c->grid);
    if (w > 2 && h > 2) {
        vs_rect(g->s, x + 1, y + 1, w - 2, h - 2, g->c->background);
        return 2u;
    }
    return 1u;
}

static uint32_t label_at(const grid* g, int32_t col, int32_t row, int32_t sx, int32_t sy, const char* text, uint32_t px) {
    int32_t y = cell_y(g, row) + (g->rh - VS_GLYPH_H * sy) / 2;
    return vs_text_xy(g->s, cell_x(g, col), y, sx, sy, text, px);
}

/* Labels of one chip band; none on a small surface (rh < 7 or cw < 8). */
static uint32_t labels(const grid* g, uint32_t chip, uint32_t px) {
    int32_t sx, sy;
    /* I950: the widest label ("Voice1", 6 glyphs) must stay inside its 7 columns, and the glyph pixel is square on screen. */
    vs_glyph_scale(g->s, g->rh, g->cw / 8, &sx, &sy);
    uint32_t v, i, n = 0;
    /* I1466: the longest word ("Triangle") needs a glyph pixel of at most cw / 8 columns; a tall-pixel surface at a narrow cw
     * rounds sx up past that, so the words are left out rather than abbreviated. */
    if (g->rh < VS_GLYPH_H + 1 || g->cw < 8 || sx > g->cw / 8) { /* I918: a glyph needs its rows plus a blank row; at rh 6 the text touched the cells and was unreadable */
        return 0;
    }
    for (v = 0; v < 3; v++) {
        int32_t base = (int32_t)v * VOICE_COLS;
        n += label_at(g, base, 0, sx, sy, k_headings[chip * 3u + v], px);
        for (i = 0; i < sizeof k_voice_labels / sizeof k_voice_labels[0]; i++) {
            n += label_at(g, base + k_voice_labels[i].col, k_voice_labels[i].row, sx, sy, k_voice_labels[i].text, px);
        }
    }
    for (i = 0; i < sizeof k_chip_labels / sizeof k_chip_labels[0]; i++) {
        n += label_at(g, k_chip_labels[i].col, k_chip_labels[i].row, sx, sy, k_chip_labels[i].text, px);
    }
    return n;
}

static uint32_t chip_draw(grid* g, uint32_t chip, int32_t bh, const sel_viz_sid_chip* sc) {
    uint32_t v, i, n = 0;
    int32_t env_h = bh * 55 / 64; /* HippoPlayer: 55 px of a 64 px band */
    int tiny = g->rh < 2;
    if (tiny) {
        g->rh = bh / 2 > 0 ? bh / 2 : 1; /* frequency bar and envelope bar only */
    }
    n += labels(g, chip, g->c->accent);
    for (v = 0; v < 3; v++) {
        const uint8_t* vr = &sc->reg[v * 7u];
        int32_t base = (int32_t)v * VOICE_COLS;
        int32_t eh = (int32_t)sc->env[v] * env_h / 64;
        n += bar(g, base + 7, tiny ? 0 : 1, 5, freq_scaled(vr), 0xFFFFu);
        if (eh > 0) {
            vs_rect(g->s, cell_x(g, base + 12), g->y0 + env_h - eh, g->cw, eh, g->c->primary);
            n++;
        }
        if (tiny) {
            continue;
        }
        n += bar(g, base + 7, 2, 5, pulse_width(vr), 0xFFFu);
        n += block(g, base + 11, 0, (sc->reg[0x17] >> v) & 1, sc->glow[v][8]);
        for (i = 0; i < 8; i++) {
            n += block(g, base + k_flags[i].col, k_flags[i].row, (vr[4] >> k_flags[i].bit) & 1,
                       sc->glow[v][k_flags[i].bit]);
        }
    }
    if (!tiny) {
        n += bar(g, 5, 7, 5, cutoff(sc->reg), 0x7FFu);
        n += bar(g, 17, 7, 2, (uint32_t)(sc->reg[0x17] >> 4), 15u);
        for (i = 0; i < 3; i++) {
            n += block(g, 19 + 7 * (int32_t)i, 7, (sc->reg[0x18] >> (4 + i)) & 1, sc->mode_glow[i]);
        }
    }
    return n;
}

/* ---- The app-design path (I1230): the host hands a pen (sel_viz_pen) with its own font, theme radius and roles. ----
 * Per chip band: three voice cards side by side (heading "Voice n" with a "Filter" chip lit while the voice is routed to
 * the filter, Frequency and Pulse width bars, the eight waveform and control chips with afterglow, a vertical envelope
 * bar), and below them one filter card (Cutoff and Resonance bars, Low pass / Band pass / High pass chips). Labels are
 * full words in the app font; lit parts use the accent (primary), unlit ones the theme track. */
static const char* const k_flag_names[8] = {"Gate", "Sync", "Ring", "Test", "Triangle", "Sawtooth", "Pulse", "Noise"};
static const char* const k_mode_names[3] = {"Low pass", "Band pass", "High pass"};
static const char* const k_voice_names[9] = {"Voice 1", "Voice 2", "Voice 3", "Voice 4", "Voice 5",
                                             "Voice 6", "Voice 7", "Voice 8", "Voice 9"};

typedef struct pen_ctx {
    const sel_viz_pen* p;
    const sel_viz_colors* c;
    int32_t gap, rh, label_w, small_r, words; /* words: the rows are tall enough for labels */
    int32_t cols;                              /* flag chip columns per voice card: 4, or 2 when 4 do not fit */
    uint32_t prims;
} pen_ctx;

static void pen_box(pen_ctx* k, int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t px) {
    if (w <= 0 || h <= 0) {
        return;
    }
    if (r * 2 > h) {
        r = h / 2;
    }
    if (r * 2 > w) {
        r = w / 2;
    }
    k->p->round_rect(k->p->ctx, x, y, w, h, r, px);
    k->prims++;
}

static void pen_word(pen_ctx* k, int32_t x, int32_t y, int32_t max_w, const char* s, uint32_t px, int strong) {
    if (k->words && max_w > 0) {
        k->prims += k->p->draw_text(k->p->ctx, x, y, max_w, s, px, strong) > 0u ? 1u : 0u;
    }
}

/* Widest of n words taken every step entries from names, in the label font. */
static int32_t pen_words_w(pen_ctx* k, const char* const* names, int32_t n, int32_t step) {
    int32_t i, w, best = 0;
    for (i = 0; i < n; i++) {
        w = k->p->text_width(k->p->ctx, names[i * step], 0);
        best = w > best ? w : best;
    }
    return best;
}

/* Text centred in a box of the row height. */
static void pen_word_mid(pen_ctx* k, int32_t x, int32_t y, int32_t w, const char* s, uint32_t px) {
    int32_t tw = k->words ? k->p->text_width(k->p->ctx, s, 0) : 0;
    int32_t pad = k->gap / 2;
    int32_t tx = tw < w - 2 * pad ? x + (w - tw) / 2 : x + pad;
    pen_word(k, tx, y + (k->rh - k->p->line_h) / 2, w - (tx - x) - pad, s, px, 0);
}

/* Label, then a rounded bar filled value / max in the accent. */
static void pen_bar(pen_ctx* k, int32_t x, int32_t y, int32_t w, const char* name, uint32_t value, uint32_t max) {
    int32_t lw = k->words ? k->label_w : 0;
    int32_t bh = k->rh / 3 < 3 ? (k->rh < 3 ? k->rh : 3) : k->rh / 3;
    int32_t by = y + (k->rh - bh) / 2, bw = w - lw, fill;
    pen_word(k, x, y + (k->rh - k->p->line_h) / 2, lw - k->gap / 2, name, k->p->muted, 0);
    pen_box(k, x + lw, by, bw, bh, k->small_r, k->p->track);
    fill = bw > 0 ? (int32_t)((uint32_t)bw * (value > max ? max : value) / max) : 0;
    if (fill > 0) {
        pen_box(k, x + lw, by, fill < bh ? bh : fill, bh, k->small_r, k->c->primary);
    }
}

/* A chip: lit (accent fill, on-accent text), fading afterglow (track towards accent), or off (track, muted text). */
static void pen_chip(pen_ctx* k, int32_t x, int32_t y, int32_t w, const char* name, int on, uint32_t glow) {
    uint32_t fill = k->p->track, ink = k->p->muted;
    if (on) {
        fill = k->c->primary;
        ink = k->p->on_accent;
    } else if (glow > 0u) {
        fill = k->p->mix(k->p->ctx, k->p->track, k->c->primary, glow * 40u);
        ink = k->p->text;
    }
    pen_box(k, x, y, w, k->rh, k->small_r, fill);
    pen_word_mid(k, x, y, w, name, ink);
}

static void pen_voice(pen_ctx* k, int32_t x, int32_t y, int32_t w, int32_t h, const sel_viz_sid_chip* sc, uint32_t v,
                      uint32_t number, int live) {
    const uint8_t* vr = sc->reg + 7u * v;
    int32_t g = k->gap, cx = x + g, cy = y + g, ew = k->rh / 3 < 4 ? 4 : k->rh / 3;
    int32_t cw = w - 3 * g - ew, fw, i, eh, ey, el, nc = k->cols, spare = cw - (nc - 1) * (g / 2), colx[4], colw[4];
    int32_t chip_w = spare / nc;
    /* I1462: each chip column is as wide as its longest word plus the chip padding, and the room left is shared out;
     * equal quarters cut "Triangle" and "Sawtooth" at 1280 wide (pen_band picks 2 columns when 4 do not fit). */
    for (i = 0; i < nc; i++) {
        colw[i] = k->words ? pen_words_w(k, k_flag_names + i, 8 / nc, nc) + g : chip_w;
        if (k->words) {
            spare -= colw[i];
        }
    }
    for (i = 0; i < nc; i++) {
        colw[i] += k->words ? spare / nc + (i == nc - 1 ? spare % nc : 0) : 0;
        colx[i] = i == 0 ? cx : colx[i - 1] + colw[i - 1] + g / 2;
    }
    pen_box(k, x, y, w, h, k->p->radius, k->p->card);
    /* heading row: name left, Filter chip right */
    fw = k->words ? k->p->text_width(k->p->ctx, "Filter", 0) + 2 * g : chip_w;
    pen_word(k, cx, cy + (k->rh - k->p->line_h) / 2, cw - fw - g, k_voice_names[number], live ? k->p->text : k->p->muted,
             1);
    pen_chip(k, cx + cw - fw, cy, fw, "Filter", live && ((sc->reg[0x17] >> v) & 1), live ? sc->glow[v][8] : 0u);
    cy += k->rh + g;
    pen_bar(k, cx, cy, cw, "Frequency", live ? freq_scaled(vr) : 0u, 0xFFFFu);
    cy += k->rh + g;
    pen_bar(k, cx, cy, cw, "Pulse width", live ? pulse_width(vr) : 0u, 0xFFFu);
    cy += k->rh + g;
    for (i = 0; i < 8; i++) {
        int32_t col = i % nc, row = i / nc;
        pen_chip(k, colx[col], cy + row * (k->rh + g / 2), colw[col], k_flag_names[i],
                 live && ((vr[4] >> i) & 1), live ? sc->glow[v][i] : 0u);
    }
    /* envelope: a vertical track on the right edge, filled from the bottom */
    ey = y + g;
    eh = h - 2 * g;
    pen_box(k, x + w - g - ew, ey, ew, eh, k->small_r, k->p->track);
    el = live ? eh * (int32_t)(sc->env[v] > 63u ? 63u : sc->env[v]) / 63 : 0;
    if (el > 0) {
        if (el < ew) {
            el = ew;
        }
        pen_box(k, x + w - g - ew, ey + eh - el, ew, el, k->small_r, k->c->primary);
    }
}

static void pen_filter(pen_ctx* k, int32_t x, int32_t y, int32_t w, int32_t h, const sel_viz_sid_chip* sc, int live) {
    int32_t g = k->gap, cx = x + g, cy = y + g, cw = w - 2 * g, chip_w, bars_w, i;
    chip_w = k->words ? pen_words_w(k, k_mode_names, 3, 1) + 2 * g : cw / 9; /* I1462: the widest mode word */
    bars_w = cw - 3 * chip_w - 3 * g;
    pen_box(k, x, y, w, h, k->p->radius, k->p->card);
    pen_bar(k, cx, cy, bars_w * 3 / 5 - g, "Cutoff", live ? cutoff(sc->reg) : 0u, 0x7FFu);
    pen_bar(k, cx + bars_w * 3 / 5, cy, bars_w * 2 / 5, "Resonance", live ? (uint32_t)(sc->reg[0x17] >> 4) : 0u, 15u);
    for (i = 0; i < 3; i++) {
        pen_chip(k, cx + bars_w + g + i * (chip_w + g), cy, chip_w, k_mode_names[i],
                 live && ((sc->reg[0x18] >> (4 + i)) & 1), live ? sc->mode_glow[i] : 0u);
    }
}

/* One chip band at y0, height bh: three voice cards over one filter card, centred when the rows reach their cap. */
static void pen_band(pen_ctx* k, const sel_surface* s, int32_t y0, int32_t bh, const sel_viz_sid_chip* sc, uint32_t chip,
                     int live) {
    const sel_viz_pen* p = k->p;
    int32_t g = p->line_h / 2 < 2 ? 2 : p->line_h / 2, w = (int32_t)s->w - 2 * g, h = bh - 2 * g, vw, vh, top, i;
    if ((h - 9 * g) / 6 < p->line_h) {
        /* too short for labels (the 72 px viz on the now-playing panel): tighter gaps, shapes only */
        g = bh / 24 < 1 ? 1 : (bh / 24 > g ? g : bh / 24);
        h = bh - 2 * g;
        w = (int32_t)s->w - 2 * g;
    }
    k->gap = g;
    k->rh = (h - 9 * g) / 6;
    if (k->rh > p->line_h * 2) {
        k->rh = p->line_h * 2; /* rows stay compact; the rest of the band is margin */
    }
    if (k->rh < 2 || w < 3 * g + 30) {
        return;
    }
    k->words = k->rh >= p->line_h;
    k->small_r = p->radius / 2;
    k->label_w = p->text_width(p->ctx, "Pulse width", 0) + g;
    vw = (w - 2 * g) / 3;
    k->cols = 4;
    if (k->words) {
        /* I1462: 4 chip columns need their longest words side by side; when the voice card (about 218 px at the
         * 1280 x 800 default) is narrower, 4 rows of 2, if the band is tall enough for them */
        int32_t ew = k->rh / 3 < 4 ? 4 : k->rh / 3, need = 3 * (g / 2);
        for (i = 0; i < 4; i++) {
            need += pen_words_w(k, k_flag_names + i, 2, 4) + g;
        }
        if (need > vw - 3 * g - ew && 7 * k->rh + 5 * g + 3 * (g / 2) + g + k->rh + 2 * g <= h) {
            k->cols = 2;
        }
    }
    /* card padding, heading, two bars, 8 / cols chip rows */
    vh = (3 + 8 / k->cols) * k->rh + 5 * g + (8 / k->cols - 1) * (g / 2);
    top = y0 + g + (h - (vh + g + k->rh + 2 * g)) / 2;
    for (i = 0; i < 3; i++) {
        pen_voice(k, g + i * (vw + g), top, vw, vh, sc, (uint32_t)i, chip * 3u + (uint32_t)i, live);
    }
    pen_filter(k, g, top + vh + g, w, k->rh + 2 * g, sc, live);
}

static uint32_t pen_render(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c, uint32_t chips) {
    static const sel_viz_sid_chip k_idle;
    pen_ctx k;
    uint32_t i;
    int32_t bh;
    k.p = s->pen;
    k.c = c;
    k.prims = 0u;
    if (chips == 0u) {
        /* no register feed (not a SID, paused, stopped): the layout unlit, nothing counted */
        pen_band(&k, s, 0, (int32_t)s->h, &k_idle, 0u, 0);
        return 0u;
    }
    bh = (int32_t)(s->h / chips);
    for (i = 0; i < chips; i++) {
        pen_band(&k, s, (int32_t)i * bh, bh, &f->sid[i], i, 1);
    }
    return k.prims;
}

static uint32_t render(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c) {
    uint32_t chips, k, prims = 0;
    int32_t bh;
    grid g;
    vs_clear(s, c->background);
    if (f == NULL || s->w < COLS || s->h < ROWS) {
        return 0;
    }
    g.s = s;
    g.c = c;
    g.cw = (int32_t)s->w / COLS;
    chips = f->sid_chips > SEL_VIZ_SID_CHIPS ? SEL_VIZ_SID_CHIPS : f->sid_chips;
    if (s->pen != NULL && s->pen->draw_text != NULL && s->pen->round_rect != NULL && s->pen->text_width != NULL &&
        s->pen->mix != NULL) {
        return pen_render(f, s, c, chips);
    }
    if (chips == 0u) {
        /* no register feed (not a SID, paused, stopped): the labels greyed, nothing counted */
        g.y0 = 0;
        g.rh = (int32_t)s->h / ROWS;
        (void)labels(&g, 0u, c->grid);
        return 0;
    }
    bh = (int32_t)(s->h / chips);
    for (k = 0; k < chips; k++) {
        g.y0 = (int32_t)k * bh;
        g.rh = bh / ROWS;
        prims += chip_draw(&g, k, bh, &f->sid[k]);
    }
    return prims;
}

const sel_tag* SEL_PLUGIN_ENTRY(uint32_t host_api) {
    if (host_api < 1u) {
        return NULL;
    }
    return vs_descriptor("SID PatternScope", "viz_sidregs", render);
}
