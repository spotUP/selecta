/* dui-font-tall (I538): the AGA atlases are baked for 1:2 pixels (ydiv 2). Horizontal coverage stays antialiased, but
 * no pixel may carry vertical partial coverage: an intermediate alpha is allowed only where the row changes
 * horizontally (a left or right neighbour with a different alpha). A flat edge, where a stroke ends at a row boundary
 * and the pixel is neither full nor empty while both horizontal neighbours match it, is vertical blending.
 * The ydiv 1 atlases (Mac, SDL, RTG) are guarded by a hash of every alpha page against a golden taken before I538. */
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GOLDEN_YDIV1_HASH 0x9b4e533b6ca760bbull /* I538: filled from the pre-change builder */

static uint64_t fnv(uint64_t h, const unsigned char* p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        h = (h ^ p[i]) * 1099511628211ull;
    }
    return h;
}

int main(void) {
    static const int tall_px[] = {22, 32};
    static const int flat_px[] = {13, 14, 15, 16, 22, 32};
    int bad = 0, k, partial_total = 0;
    uint64_t h = 1469598103934665603ull;
    for (k = 0; k < 2; k++) {
        sel_gui_atlas* a = desk_atlas_build_tall(sel_font_inter_semibold, tall_px[k], 2);
        const sel_gui_atlas* b = desk_atlas_baked(DESK_FAMILY_INTER_SEMIBOLD, tall_px[k], 2);
        uint32_t g;
        int flat = 0, partial = 0;
        if (a == NULL || b == NULL) {
            fprintf(stderr, "font_tall_unit: no ydiv 2 atlas at %d\n", tall_px[k]);
            return 1;
        }
        for (g = 0; g < a->glyph_n; g++) {
            const sel_gui_glyph* gl = &a->glyphs[g];
            int x, y;
            for (y = 0; y < gl->h; y++) {
                for (x = 0; x < gl->w; x++) {
                    const unsigned char* row = a->alpha + (size_t)(gl->y + y) * a->page_w + gl->x;
                    int v = row[x], l = x > 0 ? row[x - 1] : 0, r = x + 1 < gl->w ? row[x + 1] : 0;
                    if (v > 0 && v < 255) {
                        partial++;
                        if (l == v && r == v) {
                            flat++;
                        }
                    }
                }
            }
        }
        if (partial == 0) {
            fprintf(stderr, "font_tall_unit: px %d has no horizontal antialiasing at all\n", tall_px[k]);
            bad++;
        }
        if (flat != 0) {
            fprintf(stderr, "font_tall_unit: px %d ydiv 2: %d flat partial-coverage pixels (vertical blending)\n", tall_px[k], flat);
            bad++;
        }
        partial_total += partial;
        /* the baked table is the same bake */
        if (b->page_w != a->page_w || b->page_h != a->page_h ||
            fnv(0, b->alpha, (size_t)b->page_w * (size_t)b->page_h) != fnv(0, a->alpha, (size_t)a->page_w * (size_t)a->page_h)) {
            fprintf(stderr, "font_tall_unit: baked px %d differs from the builder\n", tall_px[k]);
            bad++;
        }
        desk_atlas_free(a);
    }
    for (k = 0; k < 6; k++) {
        int w;
        for (w = 0; w < 2; w++) {
            sel_gui_atlas* a = desk_atlas_build_tall(w ? sel_font_inter_semibold : sel_font_inter_regular, flat_px[k], 1);
            if (a == NULL) {
                return 1;
            }
            h = fnv(h, a->alpha, (size_t)a->page_w * (size_t)a->page_h);
            desk_atlas_free(a);
        }
    }
    printf("font_tall_unit ydiv1_hash=0x%llx partial=%d bad=%d\n", (unsigned long long)h, partial_total, bad);
    if (GOLDEN_YDIV1_HASH != 0ull && h != GOLDEN_YDIV1_HASH) {
        fprintf(stderr, "font_tall_unit: ydiv 1 atlases changed (hash 0x%llx)\n", (unsigned long long)h);
        bad++;
    }
    return bad != 0;
}
