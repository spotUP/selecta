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
    if (sel_font_baked_n == 0u || desk_atlas_baked(1, 13) == NULL || desk_atlas_baked(0, 13) == NULL) {
        fprintf(stderr, "font_baked_unit: nothing baked (n=%u)\n", (unsigned)sel_font_baked_n);
        return 1;
    }
    for (e = 0; e < sel_font_baked_n; e++) {
        const desk_baked_font* b = &sel_font_baked[e];
        sel_gui_atlas* r = desk_atlas_build(b->semibold ? sel_font_inter_semibold : sel_font_inter_regular, b->px);
        const sel_gui_atlas* a = &b->at;
        int ok = r != NULL && a->glyph_n == r->glyph_n && a->kern_n == r->kern_n && a->page_w == r->page_w &&
                 a->page_h == r->page_h && a->ascent == r->ascent && a->line_height == r->line_height &&
                 same_glyph(&a->missing, &r->missing) && memcmp(a->alpha, r->alpha, (size_t)a->page_w * (size_t)a->page_h) == 0;
        for (i = 0; ok && i < a->glyph_n; i++) {
            ok = same_glyph(&a->glyphs[i], &r->glyphs[i]);
        }
        for (i = 0; ok && i < a->kern_n; i++) {
            ok = a->kerns[i].a == r->kerns[i].a && a->kerns[i].b == r->kerns[i].b && a->kerns[i].amount == r->kerns[i].amount;
        }
        if (!ok) {
            fprintf(stderr, "font_baked_unit: weight %d size %d differs from the runtime atlas\n", (int)b->semibold, (int)b->px);
            bad++;
        }
        compared++;
        desk_atlas_free(r);
    }
    printf("font_baked_unit compared=%d bad=%d\n", compared, bad);
    return bad != 0;
}
