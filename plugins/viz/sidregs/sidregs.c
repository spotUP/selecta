/* SID PatternScope viz plugin (I434j2, design 2026-11-01-sid-register-scope.md section 2.5): the HippoPlayer register view.
 * Per SID chip a 40 x 8 cell band: per voice the filter route, frequency and pulse width bars, the eight control flags
 * (with afterglow), a vertical envelope bar; per chip the filter cutoff, resonance and LP/BP/HP row. The raw registers
 * and the glow counters come from the engine (sel_viz_sid_chip); all decoding lives here. */
#include "viz/glyphs.h"

#define COLS 40
#define ROWS 8
#define VOICE_COLS 13 /* voice v starts at column 13 * v */

typedef struct label {
    uint8_t col, row;
    const char* text;
} label;

/* Per voice, relative to the voice column (the heading "Voice<n>" is per chip). */
static const label k_voice_labels[] = {
    {7, 0, "Flt"},  {0, 1, "Fr"},   {0, 2, "Pw"},   {0, 3, "Gate"}, {6, 3, "Tri"},  {0, 4, "Sync"},
    {6, 4, "Saw"},  {0, 5, "Ring"}, {6, 5, "Pul"},  {0, 6, "Test"}, {6, 6, "Noi"},
};
/* Per chip. */
static const label k_chip_labels[] = {
    {0, 7, "Filter"}, {16, 7, "Reso"}, {26, 7, "LP"}, {31, 7, "BP"}, {36, 7, "HP"},
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

static uint32_t label_at(const grid* g, int32_t col, int32_t row, int32_t scale, const char* text, uint32_t px) {
    int32_t y = cell_y(g, row) + (g->rh - VS_GLYPH_H * scale) / 2;
    return vs_text(g->s, cell_x(g, col), y, scale, text, px);
}

/* Labels of one chip band; none on a small surface (rh < 6 or cw < 3). */
static uint32_t labels(const grid* g, uint32_t chip, uint32_t px) {
    int32_t scale = g->rh / 7 > 1 ? g->rh / 7 : 1;
    uint32_t v, i, n = 0;
    if (g->rh < VS_GLYPH_H + 1 || g->cw < 3) { /* I918: a glyph needs its rows plus a blank row; at rh 6 the text touched the cells and was unreadable */
        return 0;
    }
    for (v = 0; v < 3; v++) {
        int32_t base = (int32_t)v * VOICE_COLS;
        n += label_at(g, base, 0, scale, k_headings[chip * 3u + v], px);
        for (i = 0; i < sizeof k_voice_labels / sizeof k_voice_labels[0]; i++) {
            n += label_at(g, base + k_voice_labels[i].col, k_voice_labels[i].row, scale, k_voice_labels[i].text, px);
        }
    }
    for (i = 0; i < sizeof k_chip_labels / sizeof k_chip_labels[0]; i++) {
        n += label_at(g, k_chip_labels[i].col, k_chip_labels[i].row, scale, k_chip_labels[i].text, px);
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
        n += bar(g, base + 3, tiny ? 0 : 1, 8, freq_scaled(vr), 0xFFFFu);
        if (eh > 0) {
            vs_rect(g->s, cell_x(g, base + 12), g->y0 + env_h - eh, g->cw, eh, g->c->primary);
            n++;
        }
        if (tiny) {
            continue;
        }
        n += bar(g, base + 3, 2, 8, pulse_width(vr), 0xFFFu);
        n += block(g, base + 11, 0, (sc->reg[0x17] >> v) & 1, sc->glow[v][8]);
        for (i = 0; i < 8; i++) {
            n += block(g, base + k_flags[i].col, k_flags[i].row, (vr[4] >> k_flags[i].bit) & 1,
                       sc->glow[v][k_flags[i].bit]);
        }
    }
    if (!tiny) {
        n += bar(g, 7, 7, 8, cutoff(sc->reg), 0x7FFu);
        n += bar(g, 21, 7, 2, (uint32_t)(sc->reg[0x17] >> 4), 15u);
        for (i = 0; i < 3; i++) {
            n += block(g, 28 + 5 * (int32_t)i, 7, (sc->reg[0x18] >> (4 + i)) & 1, sc->mode_glow[i]);
        }
    }
    return n;
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
