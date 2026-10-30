/* Atlas builder (DUI-P2-3); see atlas_build.h. */
#include "atlas_build.h"

/* Only tinfl is used; the zlib-name shims in miniz.h are unused statics (-Werror). selecta_archive sets this for the
   real build; the SDK check target has no such link, so the file states it itself (I720). */
#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include <miniz.h>

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnewline-eof"
#endif
#include "stb_truetype.h"
#ifdef __clang__
#pragma GCC diagnostic pop
#endif

#include <stdlib.h>
#include <string.h>

#define PAGE_W 1024

typedef struct range {
    uint32_t lo, hi;
} range;

static const range g_ranges[] = {
    {0x20u, 0x7Eu},     /* Basic Latin */
    {0xA0u, 0x17Fu},    /* Latin-1 Supplement, Latin Extended-A */
    {0x2013u, 0x2014u}, /* en dash, em dash */
    {0x2018u, 0x2019u}, /* single quotes */
    {0x201Cu, 0x201Du}, /* double quotes */
    {0x2022u, 0x2022u}, /* bullet */
    {0x2026u, 0x2026u}, /* ellipsis */
};

static int cmp_kern(const void* a, const void* b) {
    const sel_gui_kern* x = (const sel_gui_kern*)a;
    const sel_gui_kern* y = (const sel_gui_kern*)b;
    if (x->a != y->a) {
        return x->a < y->a ? -1 : 1;
    }
    return x->b < y->b ? -1 : x->b > y->b;
}

#define TALL_SUB 8 /* vertical sub-rows per screen row when baking for tall pixels */

/* I538: rasterise a glyph for a screen whose pixels are ydiv times taller than wide. Coverage stays antialiased
 * across the columns, but each screen row is either in or out per column: the glyph is rendered by the same stb
 * rasteriser at TALL_SUB sub-rows per screen row, a row is in when at least half of its sub-rows cover the column
 * (alpha >= 128), and then carries the strongest horizontal coverage of those sub-rows. No vertical blending. */
static void raster_tall(const stbtt_fontinfo* f, float scale, int ydiv, int gi, int y0, unsigned char* dst, int w, int h) {
    float sy = scale / (float)ydiv;
    float sh = sy * (float)TALL_SUB;
    int hx0, hy0, hx1, hy1, hh, r, c, k;
    unsigned char* hi;
    stbtt_GetGlyphBitmapBox(f, gi, scale, sh, &hx0, &hy0, &hx1, &hy1);
    hh = hy1 - hy0;
    hi = (unsigned char*)calloc((size_t)w * (size_t)(hh > 0 ? hh : 1), 1);
    if (hi == NULL) {
        stbtt_MakeGlyphBitmap(f, dst, w, h, PAGE_W, scale, sy, gi);
        return;
    }
    stbtt_MakeGlyphBitmap(f, hi, w, hh, w, scale, sh, gi);
    for (r = 0; r < h; r++) {
        for (c = 0; c < w; c++) {
            int on = 0, mx = 0;
            for (k = 0; k < TALL_SUB; k++) {
                int hr = (y0 + r) * TALL_SUB + k - hy0; /* screen row r spans sub-rows y0*SUB + r*SUB .. +SUB-1 */
                int a = (hr >= 0 && hr < hh) ? hi[(size_t)hr * (size_t)w + (size_t)c] : 0;
                if (a >= 128) {
                    on++;
                }
                if (a > mx) {
                    mx = a;
                }
            }
            dst[(size_t)r * PAGE_W + (size_t)c] = (unsigned char)(on * 2 >= TALL_SUB ? mx : 0);
        }
    }
    free(hi);
}

/* Fills `g` from glyph index `gi`; the bitmap is rendered into `page` when it is not NULL, at the shelf position
 * the caller keeps in *px, *py, *row. */
static void place(const stbtt_fontinfo* f, float scale, int ydiv, int gi, uint32_t cp, sel_gui_glyph* g, unsigned char* page,
                  int* px, int* py, int* row) {
    int x0, y0, x1, y1, adv, lsb, w, h;
    float sy = scale / (float)ydiv; /* I468: rows are ydiv times taller than columns, so the bitmap has 1/ydiv the rows */
    stbtt_GetGlyphBitmapBox(f, gi, scale, sy, &x0, &y0, &x1, &y1);
    stbtt_GetGlyphHMetrics(f, gi, &adv, &lsb);
    w = x1 - x0;
    h = y1 - y0;
    memset(g, 0, sizeof *g);
    g->cp = cp;
    g->adv = (uint16_t)((float)adv * scale + 0.5f);
    g->bx = (int16_t)x0;
    g->by = (int16_t)(y0 * ydiv); /* layout units: the draw halves the pen row (gui_draw ydiv) */
    if (w <= 0 || h <= 0) {
        return;
    }
    if (*px + w + 1 > PAGE_W) {
        *px = 0;
        *py += *row + 1;
        *row = 0;
    }
    g->x = (uint16_t)*px;
    g->y = (uint16_t)*py;
    g->w = (uint16_t)w;
    g->h = (uint16_t)h;
    if (page != NULL) {
        if (ydiv > 1) {
            raster_tall(f, scale, ydiv, gi, y0, page + (size_t)*py * PAGE_W + (size_t)*px, w, h);
        } else {
            stbtt_MakeGlyphBitmap(f, page + (size_t)*py * PAGE_W + (size_t)*px, w, h, PAGE_W, scale, sy, gi);
        }
    }
    *px += w + 1;
    if (h > *row) {
        *row = h;
    }
}

sel_gui_atlas* desk_atlas_build(const unsigned char* ttf, int pixel_size) {
    return desk_atlas_build_tall(ttf, pixel_size, 1);
}

sel_gui_atlas* desk_atlas_build_tall(const unsigned char* ttf, int pixel_size, int ydiv) {
    stbtt_fontinfo f;
    sel_gui_atlas* at;
    sel_gui_glyph* glyphs;
    sel_gui_kern* kerns;
    unsigned char* page;
    uint32_t n = 0, cap = 0, r, cp, nk = 0, i, j;
    int px, py, row, pass, asc, desc, gap;
    float scale;
    if (!stbtt_InitFont(&f, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        return NULL;
    }
    for (r = 0; r < sizeof g_ranges / sizeof g_ranges[0]; r++) {
        cap += g_ranges[r].hi - g_ranges[r].lo + 1u;
    }
    at = (sel_gui_atlas*)calloc(1, sizeof *at);
    glyphs = (sel_gui_glyph*)calloc(cap, sizeof *glyphs);
    kerns = (sel_gui_kern*)calloc(95u * 95u, sizeof *kerns);
    if (at == NULL || glyphs == NULL || kerns == NULL) {
        free(at);
        free(glyphs);
        free(kerns);
        return NULL;
    }
    scale = stbtt_ScaleForMappingEmToPixels(&f, (float)pixel_size);
    stbtt_GetFontVMetrics(&f, &asc, &desc, &gap);
    at->ascent = (int32_t)((float)asc * scale + 0.5f);
    at->line_height = (int32_t)((float)(asc - desc + gap) * scale + 0.5f);
    page = NULL;
    for (pass = 0; pass < 2; pass++) {
        px = py = row = 0;
        n = 0;
        place(&f, scale, ydiv, 0, 0xFFFFFFFFu, &at->missing, page, &px, &py, &row);
        for (r = 0; r < sizeof g_ranges / sizeof g_ranges[0]; r++) {
            for (cp = g_ranges[r].lo; cp <= g_ranges[r].hi; cp++) {
                int gi = stbtt_FindGlyphIndex(&f, (int)cp);
                if (gi == 0) {
                    continue;
                }
                place(&f, scale, ydiv, gi, cp, &glyphs[n], page, &px, &py, &row);
                n++;
            }
        }
        if (pass == 0) {
            at->page_w = PAGE_W;
            at->page_h = py + row + 1;
            page = (unsigned char*)calloc((size_t)at->page_w * (size_t)at->page_h, 1);
            if (page == NULL) {
                free(at);
                free(glyphs);
                free(kerns);
                return NULL;
            }
        }
    }
    for (i = 0x20u; i <= 0x7Eu; i++) {
        for (j = 0x20u; j <= 0x7Eu; j++) {
            int k = stbtt_GetGlyphKernAdvance(&f, stbtt_FindGlyphIndex(&f, (int)i), stbtt_FindGlyphIndex(&f, (int)j));
            int amount = (int)((float)k * scale + (k < 0 ? -0.5f : 0.5f));
            if (amount != 0) {
                kerns[nk].a = i;
                kerns[nk].b = j;
                kerns[nk].amount = (int16_t)amount;
                nk++;
            }
        }
    }
    qsort(kerns, nk, sizeof *kerns, cmp_kern);
    at->glyphs = glyphs;
    at->glyph_n = n;
    at->kerns = kerns;
    at->kern_n = nk;
    at->alpha = page;
    return at;
}

/* I587: the decoded pages. The table stores each page packed; a page is expanded the first time its atlas is asked for
 * and then kept for the life of the process (desk_atlas_free leaves it alone), parallel to sel_font_baked. */
static sel_gui_atlas* g_decoded; /* pages are raw deflate (tools/gui_fontbake.c), inflated with the miniz tinfl the archive adapter links */

const sel_gui_atlas* desk_atlas_baked(int family, int pixel_size, int ydiv) {
    uint32_t i;
    for (i = 0; i < sel_font_baked_n; i++) {
        const desk_baked_font* b = &sel_font_baked[i];
        if (b->family == family && b->px == pixel_size && b->ydiv == ydiv) {
            size_t total = (size_t)b->at.page_w * (size_t)b->at.page_h;
            uint8_t* page;
            if (g_decoded == NULL) {
                g_decoded = (sel_gui_atlas*)calloc(sel_font_baked_n, sizeof *g_decoded);
                if (g_decoded == NULL) {
                    return NULL;
                }
            }
            if (g_decoded[i].alpha == NULL) {
                page = (uint8_t*)malloc(total);
                if (page == NULL || tinfl_decompress_mem_to_mem(page, total, b->packed, b->packed_n, 0) != total) {
                    free(page);
                    return NULL;
                }
                g_decoded[i] = b->at;
                g_decoded[i].alpha = page;
            }
            return &g_decoded[i];
        }
    }
    return NULL;
}

int32_t desk_baked_scale_snap_q16(int32_t q16) {
    static const int32_t scales[] = {DESK_FONT_BAKE_SCALES_Q16};
    int32_t best = scales[0], bd = -1;
    size_t i;
    for (i = 0; i < sizeof scales / sizeof scales[0]; i++) {
        int32_t dd = q16 > scales[i] ? q16 - scales[i] : scales[i] - q16;
        if (bd < 0 || dd < bd) {
            bd = dd;
            best = scales[i];
        }
    }
    return best;
}

int32_t desk_baked_scale_fit_q16(int32_t q16) {
    static const int32_t scales[] = {DESK_FONT_BAKE_SCALES_Q16};
    int32_t fit = -1, lowest = scales[0];
    size_t i;
    for (i = 0; i < sizeof scales / sizeof scales[0]; i++) {
        if (scales[i] <= q16 && scales[i] > fit) {
            fit = scales[i];
        }
        if (scales[i] < lowest) {
            lowest = scales[i];
        }
    }
    return fit < 0 ? lowest : fit;
}

void desk_atlas_free(const sel_gui_atlas* at) {
    if (at == NULL) {
        return;
    }
    if (g_decoded != NULL && at >= g_decoded && at < g_decoded + sel_font_baked_n) {
        return; /* a baked atlas: the decoded page is shared and kept */
    }
    free((void*)at->glyphs);
    free((void*)at->kerns);
    free((void*)at->alpha);
    free((void*)at);
}
