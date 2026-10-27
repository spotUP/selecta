/* dui-font-baked unit (DUI-P4-25, I463): every baked atlas equals the atlas the runtime builder makes for the same font and
 * pixel size, field by field (glyph table, kerning table, alpha page, metrics). The tables come from tools/gui_fontbake.c,
 * which calls the same desk_atlas_build, so this guards the bake step itself. */
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcomment"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wnewline-eof"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "atlas_build.h"

#include <stdio.h>
#include <string.h>

static int same_glyph(const sel_gui_glyph* a, const sel_gui_glyph* b) {
    return a->cp == b->cp && a->x == b->x && a->y == b->y && a->w == b->w && a->h == b->h && a->bx == b->bx &&
           a->by == b->by && a->adv == b->adv;
}

int main(void) {
    uint32_t e, i;
    int bad = 0, compared = 0;
    if (sel_font_baked_n == 0u || desk_atlas_baked(1, 13, 1) == NULL || desk_atlas_baked(0, 13, 1) == NULL) {
        fprintf(stderr, "font_baked_unit: nothing baked (n=%u)\n", (unsigned)sel_font_baked_n);
        return 1;
    }
    for (e = 0; e < sel_font_baked_n; e++) {
        const desk_baked_font* b = &sel_font_baked[e];
        sel_gui_atlas* r = b->family >= DESK_FAMILY_MINI4 ? NULL : desk_atlas_build_tall(b->family == DESK_FAMILY_INTER_SEMIBOLD ? sel_font_inter_semibold : sel_font_inter_regular, b->px, b->ydiv);
        const sel_gui_atlas* a = &b->at;
        int ok;
        if (b->family >= DESK_FAMILY_MINI4) { /* Amiga bitmap atlases are checked by dui-amiga-font */
            continue;
        }
        ok = r != NULL && a->glyph_n == r->glyph_n && a->kern_n == r->kern_n && a->page_w == r->page_w &&
                 a->page_h == r->page_h && a->ascent == r->ascent && a->line_height == r->line_height &&
                 same_glyph(&a->missing, &r->missing) && memcmp(a->alpha, r->alpha, (size_t)a->page_w * (size_t)a->page_h) == 0;
        for (i = 0; ok && i < a->glyph_n; i++) {
            ok = same_glyph(&a->glyphs[i], &r->glyphs[i]);
        }
        for (i = 0; ok && i < a->kern_n; i++) {
            ok = a->kerns[i].a == r->kerns[i].a && a->kerns[i].b == r->kerns[i].b && a->kerns[i].amount == r->kerns[i].amount;
        }
        if (!ok) {
            fprintf(stderr, "font_baked_unit: family %d size %d ydiv %d differs from the runtime atlas\n", (int)b->family, (int)b->px, (int)b->ydiv);
            bad++;
        }
        compared++;
        desk_atlas_free(r);
    }
    /* I526-P2: the ydiv 2 Inter set is SemiBold headlines only (px 18 and 24); nothing else at ydiv 2 is Inter. */
    for (e = 0; e < sel_font_baked_n; e++) {
        const desk_baked_font* b = &sel_font_baked[e];
        if (b->ydiv == 2 && b->family < DESK_FAMILY_MINI4 && !(b->family == DESK_FAMILY_INTER_SEMIBOLD && (b->px == 18 || b->px == 24))) {
            fprintf(stderr, "font_baked_unit: unexpected Inter ydiv 2 atlas family %d size %d\n", (int)b->family, (int)b->px);
            bad++;
        }
    }
    {
        static const int px[] = {24, 18};
        /* Inter cap height is 0.727 em, so an L at ydiv 2 is px x 0.364 rows: 8.7 at px 24, 6.5 at px 18. The plan's
         * 15-16 and 10-11 rows are the em height, not the ink of an L; measured 12 and 9. */
        static const int rows_lo[] = {8, 6};
        static const int rows_hi[] = {9, 7};
        int k;
        for (k = 0; k < 2; k++) {
            const sel_gui_atlas* a = desk_atlas_baked(DESK_FAMILY_INTER_SEMIBOLD, px[k], 2);
            const sel_gui_glyph* l = NULL;
            uint32_t g;
            for (g = 0; a != NULL && g < a->glyph_n; g++) {
                if (a->glyphs[g].cp == 'L') {
                    l = &a->glyphs[g];
                }
            }
            if (l == NULL || l->h < rows_lo[k] || l->h > rows_hi[k]) {
                fprintf(stderr, "font_baked_unit: semibold %d ydiv 2: 'L' is %d rows, want %d to %d\n", px[k], l == NULL ? -1 : (int)l->h, rows_lo[k], rows_hi[k]);
                bad++;
            }
        }
    }
    printf("font_baked_unit compared=%d bad=%d\n", compared, bad);
    return bad != 0;
}
