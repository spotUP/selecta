/* Tiny bitmap font of the viz plugins (I434j2): 4 x 6 glyphs, one byte per row (bit 3 = left pixel), for exactly the
 * characters the SID PatternScope labels need. No reusable viz font exists (searched plugins/shared/viz, src, plugins:
 * the other fonts are the desktop UI's atlas and amiga_font.c, host-side and UI-owned). A character without a glyph
 * draws nothing. Header-only like surface.h; include after it. */
#ifndef SELECTA_SHARED_VIZ_GLYPHS_H
#define SELECTA_SHARED_VIZ_GLYPHS_H

#include "viz/surface.h"

#define VS_GLYPH_W 4
#define VS_GLYPH_H 6
#define VS_GLYPH_ADVANCE 5 /* glyph width plus one column */

typedef struct vs_glyph {
    char ch;
    uint8_t row[VS_GLYPH_H];
} vs_glyph;

static const vs_glyph vs_glyphs[] = {
    {'1', {0x2, 0x6, 0x2, 0x2, 0x2, 0x7}}, {'2', {0x6, 0x9, 0x1, 0x6, 0x8, 0xF}},
    {'3', {0xE, 0x1, 0x6, 0x1, 0x9, 0x6}}, {'4', {0x9, 0x9, 0xF, 0x1, 0x1, 0x1}},
    {'5', {0xF, 0x8, 0xE, 0x1, 0x9, 0x6}}, {'6', {0x6, 0x8, 0xE, 0x9, 0x9, 0x6}},
    {'7', {0xF, 0x1, 0x2, 0x4, 0x4, 0x4}}, {'8', {0x6, 0x9, 0x6, 0x9, 0x9, 0x6}},
    {'9', {0x6, 0x9, 0x7, 0x1, 0x1, 0x6}}, {'V', {0x9, 0x9, 0x9, 0x9, 0x6, 0x6}},
    {'o', {0x0, 0x0, 0x6, 0x9, 0x9, 0x6}}, {'i', {0x4, 0x0, 0x4, 0x4, 0x4, 0x4}},
    {'c', {0x0, 0x0, 0x7, 0x8, 0x8, 0x7}}, {'e', {0x0, 0x6, 0x9, 0xF, 0x8, 0x7}},
    {'F', {0xF, 0x8, 0xE, 0x8, 0x8, 0x8}}, {'l', {0x4, 0x4, 0x4, 0x4, 0x4, 0x2}},
    {'t', {0x4, 0xE, 0x4, 0x4, 0x4, 0x3}}, {'r', {0x0, 0x0, 0xB, 0xC, 0x8, 0x8}},
    {'P', {0xE, 0x9, 0xE, 0x8, 0x8, 0x8}}, {'w', {0x0, 0x0, 0x9, 0x9, 0xF, 0x6}},
    {'G', {0x6, 0x9, 0x8, 0xB, 0x9, 0x6}}, {'a', {0x0, 0x6, 0x1, 0x7, 0x9, 0x7}},
    {'T', {0xF, 0x4, 0x4, 0x4, 0x4, 0x4}}, {'S', {0x7, 0x8, 0x6, 0x1, 0x1, 0xE}},
    {'y', {0x0, 0x9, 0x9, 0x7, 0x1, 0x6}}, {'n', {0x0, 0x0, 0xE, 0x9, 0x9, 0x9}},
    {'R', {0xE, 0x9, 0xE, 0xC, 0xA, 0x9}}, {'u', {0x0, 0x0, 0x9, 0x9, 0x9, 0x7}},
    {'s', {0x0, 0x0, 0x7, 0x8, 0x1, 0xE}}, {'N', {0x9, 0xD, 0xF, 0xB, 0x9, 0x9}},
    {'H', {0x9, 0x9, 0xF, 0x9, 0x9, 0x9}}, {'B', {0xE, 0x9, 0xE, 0x9, 0x9, 0xE}},
    {'L', {0x8, 0x8, 0x8, 0x8, 0x8, 0xF}}, {'g', {0x0, 0x7, 0x9, 0x7, 0x1, 0x6}},
};

/* The glyph rows of `ch`, NULL for a space or a character without a glyph. */
static inline const uint8_t* vs_glyph_rows(char ch) {
    uint32_t i;
    for (i = 0; i < (uint32_t)(sizeof vs_glyphs / sizeof vs_glyphs[0]); i++) {
        if (vs_glyphs[i].ch == ch) {
            return vs_glyphs[i].row;
        }
    }
    return NULL;
}

/* Draws `str` with its top left at (x, y), every glyph pixel `sx` wide and `sy` high (I950: sx = sy * aspect on a tall-pixel
 * screen so the glyph looks square). A horizontal run of lit pixels, merged downward through the rows that hold the same run,
 * is one vs_rect. Returns the number of rects drawn. */
static inline uint32_t vs_text_xy(sel_surface* s, int32_t x, int32_t y, int32_t sx, int32_t sy, const char* str, uint32_t px) {
    uint32_t prims = 0;
    for (; *str != '\0'; str++, x += VS_GLYPH_ADVANCE * sx) {
        const uint8_t* g = vs_glyph_rows(*str);
        uint8_t loc[VS_GLYPH_H];
        int32_t r, c, d;
        if (g == NULL) {
            continue;
        }
        for (r = 0; r < VS_GLYPH_H; r++) {
            loc[r] = g[r];
        }
        for (r = 0; r < VS_GLYPH_H; r++) {
            for (c = 0; c < VS_GLYPH_W; c++) {
                int32_t end = c, down = 1;
                uint8_t mask = 0;
                if ((loc[r] & (0x8u >> c)) == 0u) {
                    continue;
                }
                while (end + 1 < VS_GLYPH_W && (loc[r] & (0x8u >> (end + 1))) != 0u) {
                    end++;
                }
                for (d = c; d <= end; d++) {
                    mask = (uint8_t)(mask | (0x8u >> d));
                }
                while (r + down < VS_GLYPH_H && (loc[r + down] & mask) == mask) {
                    loc[r + down] = (uint8_t)(loc[r + down] & ~mask);
                    down++;
                }
                vs_rect(s, x + c * sx, y + r * sy, (end - c + 1) * sx, down * sy, px);
                prims++;
                c = end;
            }
        }
    }
    return prims;
}

/* Square glyph pixels: the original single-scale call. */
static inline uint32_t vs_text(sel_surface* s, int32_t x, int32_t y, int32_t scale, const char* str, uint32_t px) {
    return vs_text_xy(s, x, y, scale, scale, str, px);
}

/* Glyph pixel size for a cell grid (I950). Fits a glyph into `cell_rows` rows of the surface (a row is aspect_q8/256 wide) and
 * into `max_w` columns, and keeps the glyph pixel a physical square: *sx = *sy * aspect_q8 / 256 (as near as integers allow).
 * `rows_needed` is the cell height in glyph rows (VS_GLYPH_H plus the blank row). The pre-I950 code used cell_rows / 7 with no
 * width cap and one scale for both axes, which on 640x256 (aspect 512) drew glyphs twice as tall as wide. */
static inline void vs_glyph_scale(const sel_surface* surf, int32_t cell_rows, int32_t max_w, int32_t* sx, int32_t* sy) {
    int32_t a = surf->pix_aspect_q8 < 256u ? 256 : (int32_t)surf->pix_aspect_q8;
    int32_t sq = (cell_rows * a / 256) / (VS_GLYPH_H + 1); /* physical square edge the cell height allows */
    int32_t y, x;
    if (sq > max_w) {
        sq = max_w;
    }
    y = sq * 256 / a;
    y = y < 1 ? 1 : y;
    x = y * a / 256;
    *sx = x < 1 ? 1 : x;
    *sy = y;
}

#endif /* SELECTA_SHARED_VIZ_GLYPHS_H */
