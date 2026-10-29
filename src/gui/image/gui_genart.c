/* Platform covers and generated artwork (design doc 2.7, DUI-P2-5; I691 platform-covers design). Integer only. */
#include <selecta/gui/gui_genart.h>
#include <selecta/uimodel/tokens.h>

#include <string.h>

uint32_t sel_gui_str_hash(const char* s) {
    uint32_t h = 2166136261u;

    for (; *s != '\0'; s++) {
        h = (h ^ (uint8_t)*s) * 16777619u;
    }
    return h;
}

/* ---- Platform covers (I691a) ---- */

enum { COVER_TONES = 3, ART_SLOTS = 32 };

static uint32_t family_base(const uint32_t* tokens, uint32_t family) {
    if (family >= SEL_PLATFORM_FAMILY_COUNT) {
        family = SEL_PLATFORM_FAMILY_MUSIC;
    }
    if (family == SEL_PLATFORM_FAMILY_MUSIC) {
        return tokens[SEL_GUI_TOKEN_ACCENT] & 0xFFFFFFu;
    }
    return tokens[SEL_GUI_TOKEN_COVER_AMIGA + (family - 1u)] & 0xFFFFFFu;
}

uint32_t sel_gui_cover_aga_tones(void) {
    return (SEL_PLATFORM_FAMILY_COUNT * COVER_TONES + 1u <= ART_SLOTS) ? COVER_TONES : 1u;
}

uint32_t sel_gui_cover_tone(const uint32_t* tokens, uint32_t family, uint32_t tone) {
    uint32_t base = family_base(tokens, family), out = 0u, sh;

    if (tone == 0u || tone >= COVER_TONES) {
        return base;
    }
    for (sh = 0u; sh < 24u; sh += 8u) {
        uint32_t ch = (base >> sh) & 0xFFu;
        ch = (tone == 1u) ? ch * 88u / 100u : ch + (255u - ch) * 12u / 100u;
        out |= ch << sh;
    }
    return out;
}

uint32_t sel_gui_cover_tone_of(uint32_t title_hash) {
    return (title_hash ^ (title_hash >> 15)) % COVER_TONES;
}

uint32_t sel_gui_cover_palette(uint32_t out[32], const uint32_t* tokens) {
    uint32_t tones = sel_gui_cover_aga_tones(), f, t, n = 0u;
    uint32_t text = tokens[SEL_GUI_TOKEN_COVER_TEXT] & 0xFFFFFFu;

    for (f = 0u; f < SEL_PLATFORM_FAMILY_COUNT; f++) {
        for (t = 0u; t < tones; t++) {
            out[n++] = sel_gui_cover_tone(tokens, f, t);
        }
    }
    while (n < ART_SLOTS) {
        out[n++] = text; /* first spare slot holds the mark and word colour; the rest repeat it */
    }
    return tones;
}

/* The dark end of a cover gradient: the tone scaled to 45% per channel (I691f). */
static uint32_t cover_dark(uint32_t tone) {
    uint32_t out = 0xFF000000u, sh;

    for (sh = 0u; sh < 24u; sh += 8u) {
        out |= (((tone >> sh) & 0xFFu) * 45u / 100u) << sh;
    }
    return out;
}

static void box(sel_gui_canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t argb) {
    sel_gui_rect q;

    q.x = x;
    q.y = y;
    q.w = w < 1 ? 1 : w;
    q.h = h < 1 ? 1 : h;
    sel_gui_fill_rect(c, q, argb);
}

static void disc(sel_gui_canvas* c, int32_t x, int32_t y, int32_t d, uint32_t argb) {
    sel_gui_rect q;

    q.x = x;
    q.y = y;
    q.w = d < 1 ? 1 : d;
    q.h = q.w;
    sel_gui_round_rect(c, q, q.w / 2, argb);
}

/* The family mark in the square (x0, y0, s, s): solid `fg`; `bg` is the field colour for cut-outs. Only fills,
 * round rects and frames, so it holds on the INDEX8 palette. */
static void family_mark(sel_gui_canvas* c, uint32_t family, int32_t x0, int32_t y0, int32_t s, uint32_t fg, uint32_t bg) {
    sel_gui_rect q;
    int32_t i, j, k, u;

    q.x = x0;
    q.y = y0;
    q.w = s;
    q.h = s;
    switch (family) {
    case SEL_PLATFORM_FAMILY_AMIGA: {
        static const int32_t hh[4] = {8, 5, 7, 3};
        for (i = 0; i < 4; i++) {
            box(c, x0 + i * (s / 4), y0 + s - s * hh[i] / 8, s / 7, s * hh[i] / 8, fg);
        }
        break;
    }
    case SEL_PLATFORM_FAMILY_COMMODORE_64:
        for (i = 0; i < 3; i++) {
            box(c, x0, y0 + i * (s * 2 / 5), s * (3 - i) / 3, s / 5, fg);
        }
        break;
    case SEL_PLATFORM_FAMILY_ATARI: {
        static const int32_t hh[3] = {6, 4, 6};
        for (i = 0; i < 3; i++) {
            box(c, x0 + i * (s * 2 / 5), y0 + s - s / 8 - s * hh[i] / 8, s / 5, s * hh[i] / 8, fg);
        }
        box(c, x0, y0 + s - s / 8, s, s / 8, fg);
        break;
    }
    case SEL_PLATFORM_FAMILY_ZX_SPECTRUM: {
        int32_t w = s / 8 < 1 ? 1 : s / 8;
        for (j = 0; j < s; j++) {
            for (k = 0; k < 4; k++) {
                int32_t x = (s - 1 - j + k * (s / 4)) % s, ww = w;
                if (x + ww > s) {
                    ww = s - x;
                }
                box(c, x0 + x, y0 + j, ww, 1, fg);
            }
        }
        break;
    }
    case SEL_PLATFORM_FAMILY_PC_TRACKER: {
        static const uint32_t cells = 0xA5C3B9u;
        u = s / 6 < 1 ? 1 : s / 6;
        for (j = 0; j < 6; j++) {
            for (i = 0; i < 4; i++) {
                if (((cells >> (uint32_t)(j * 4 + i)) & 1u) != 0u) {
                    box(c, x0 + i * u, y0 + j * u, u > 2 ? u - 1 : u, u > 2 ? u - 1 : u, fg);
                }
            }
        }
        break;
    }
    case SEL_PLATFORM_FAMILY_NES:
        u = s / 3;
        box(c, x0, y0 + u, s, u, fg);
        box(c, x0 + u, y0, u, s, fg);
        break;
    case SEL_PLATFORM_FAMILY_GAME_BOY:
        q.x = x0 + s / 6;
        q.w = s * 2 / 3;
        sel_gui_round_rect(c, q, s / 10, fg);
        box(c, q.x + s / 10, y0 + s / 10, q.w - s / 5, s * 2 / 5, bg);
        u = s / 8;
        box(c, q.x + s / 5, y0 + s * 7 / 10, u, u, bg);
        box(c, q.x + q.w - s / 5 - u, y0 + s * 7 / 10, u, u, bg);
        break;
    case SEL_PLATFORM_FAMILY_SNES:
        u = s / 3;
        disc(c, x0 + u, y0, u, fg);
        disc(c, x0 + u, y0 + s - u, u, fg);
        disc(c, x0, y0 + u, u, fg);
        disc(c, x0 + s - u, y0 + u, u, fg);
        break;
    case SEL_PLATFORM_FAMILY_PC_ENGINE:
        sel_gui_round_frame(c, q, s / 2, s / 6 < 1 ? 1 : s / 6, fg);
        break;
    case SEL_PLATFORM_FAMILY_MSX:
        u = s / 3;
        box(c, x0, y0 + 2 * u, u, u, fg);
        box(c, x0 + u, y0 + u, u, u, fg);
        box(c, x0 + 2 * u, y0, u, u, fg);
        break;
    case SEL_PLATFORM_FAMILY_MEGA_DRIVE:
        u = s / 2;
        disc(c, x0, y0 + s / 4, u, fg);
        disc(c, x0 + s - u, y0 + s / 4, u, fg);
        break;
    case SEL_PLATFORM_FAMILY_CONSOLE:
        sel_gui_round_frame(c, q, s / 5, s / 8 < 1 ? 1 : s / 8, fg);
        break;
    default:
        disc(c, x0, y0, s, fg);
        break;
    }
}

void sel_gui_cover_draw(sel_gui_canvas* c, sel_gui_rect r, uint32_t family, uint32_t title_hash, const uint32_t* tokens,
                        const sel_gui_atlas* label_font) {
    uint32_t t0, region, tone, field, text;
    int32_t edge, margin, s;

    if (r.w <= 0 || r.h <= 0) {
        return;
    }
    if (family >= SEL_PLATFORM_FAMILY_COUNT) {
        family = SEL_PLATFORM_FAMILY_MUSIC;
    }
    t0 = sel_gui_prof_begin(c);
    region = c->region;
    c->region = SEL_GUI_RGN_ART;
    tone = (c->format == SEL_GUI_INDEX8 && sel_gui_cover_aga_tones() == 1u) ? 0u : sel_gui_cover_tone_of(title_hash);
    field = 0xFF000000u | sel_gui_cover_tone(tokens, family, tone);
    text = 0xFF000000u | (tokens[SEL_GUI_TOKEN_COVER_TEXT] & 0xFFFFFFu);
    if (c->format == SEL_GUI_INDEX8) {
        sel_gui_fill_rect(c, r, field); /* I691f: the 32 cover slots hold the tones, a gradient would leave them: AGA stays flat */
    } else {
        sel_gui_gradient(c, r, 1, cover_dark(field), field); /* I691f: dark shade at the top to the family tone at the bottom */
    }

    edge = r.w < r.h ? r.w : r.h;
    margin = edge / 10;
    s = edge / 4;
    if (s >= 2) {
        family_mark(c, family, r.x + r.w - margin - s, r.y + margin, s, text, field);
    }
    if (label_font != NULL && edge >= 64) {
        sel_gui_text_draw(c, label_font, r.x + margin, r.y + r.h - margin - label_font->line_height,
                          sel_platform_label(family), text, r.w - 2 * margin, NULL);
    }
    c->region = region;
    sel_gui_prof_end(c, SEL_GUI_PROF_GENART, t0);
}
