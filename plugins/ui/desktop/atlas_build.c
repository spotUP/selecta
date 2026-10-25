/* Atlas builder (DUI-P2-3); see atlas_build.h. */
#include "atlas_build.h"

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

/* Fills `g` from glyph index `gi`; the bitmap is rendered into `page` when it is not NULL, at the shelf position
 * the caller keeps in *px, *py, *row. */
static void place(const stbtt_fontinfo* f, float scale, int gi, uint32_t cp, sel_gui_glyph* g, unsigned char* page,
                  int* px, int* py, int* row) {
    int x0, y0, x1, y1, adv, lsb, w, h;
    stbtt_GetGlyphBitmapBox(f, gi, scale, scale, &x0, &y0, &x1, &y1);
    stbtt_GetGlyphHMetrics(f, gi, &adv, &lsb);
    w = x1 - x0;
    h = y1 - y0;
    memset(g, 0, sizeof *g);
    g->cp = cp;
    g->adv = (uint16_t)((float)adv * scale + 0.5f);
    g->bx = (int16_t)x0;
    g->by = (int16_t)y0;
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
        stbtt_MakeGlyphBitmap(f, page + (size_t)*py * PAGE_W + (size_t)*px, w, h, PAGE_W, scale, scale, gi);
    }
    *px += w + 1;
    if (h > *row) {
        *row = h;
    }
}

sel_gui_atlas* desk_atlas_build(const unsigned char* ttf, int pixel_size) {
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
        place(&f, scale, 0, 0xFFFFFFFFu, &at->missing, page, &px, &py, &row);
        for (r = 0; r < sizeof g_ranges / sizeof g_ranges[0]; r++) {
            for (cp = g_ranges[r].lo; cp <= g_ranges[r].hi; cp++) {
                int gi = stbtt_FindGlyphIndex(&f, (int)cp);
                if (gi == 0) {
                    continue;
                }
                place(&f, scale, gi, cp, &glyphs[n], page, &px, &py, &row);
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

void desk_atlas_free(sel_gui_atlas* at) {
    if (at != NULL) {
        free((void*)at->glyphs);
        free((void*)at->kerns);
        free((void*)at->alpha);
        free(at);
    }
}
