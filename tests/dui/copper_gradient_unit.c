/* I441: copper gradients. On an INDEX8 canvas with copper pens, an opaque vertical gradient fills with a reserved pen
 * instead of dithered cube indices, and sel_gui_copper_changes returns the per-line pen colours the aga backend loads
 * into a UCopList. Covers: the sentinel (copper_filled), the pen, the line colours against the drawn-gradient maths,
 * ydiv 2 row mapping, pen sharing between clipped pieces of one gradient, fallback when pens run out, and the AGA
 * register words. */
#include "amiga/aga_planar.h"

#include <selecta/gui/gui_draw.h>
#include <stdio.h>
#include <stdlib.h>

static int fails;
#define CHECK(c) \
    do { \
        if (!(c)) { \
            fprintf(stderr, "copper_gradient_unit: line %d: %s\n", __LINE__, #c); \
            fails++; \
        } \
    } while (0)

static sel_gui_canvas* make(uint8_t** px, int32_t w, int32_t h, uint32_t pens, int32_t ydiv) {
    sel_gui_canvas* c = (sel_gui_canvas*)calloc(1, sizeof *c);
    uint32_t pal[256];
    *px = (uint8_t*)calloc((size_t)w * (size_t)h, 1);
    sel_gui_canvas_init(c, SEL_GUI_INDEX8, w, h, *px);
    c->ydiv = ydiv;
    sel_gui_copper_enable(c, pens);
    sel_gui_palette_default(pal, 0x1DB954u, 0x121212u, 0xFFFFFFu);
    sel_gui_set_palette(c, pal, 0u, 256u);
    return c;
}

static sel_gui_rect rc(int32_t x, int32_t y, int32_t w, int32_t h) {
    sel_gui_rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

int main(void) {
    uint8_t* px;
    sel_gui_canvas* c = make(&px, 64, 32, 4u, 1);
    sel_gui_copper_change ch[64];
    uint32_t n, i;
    sel_gui_gradient(c, rc(0, 4, 64, 11), 1, 0xFF000000u, 0xFFFF0A64u);
    CHECK(c->copper_filled == 11u);
    CHECK(px[4 * 64] == 255 && px[14 * 64 + 63] == 255 && px[3 * 64] == 0 && px[15 * 64] == 0);
    n = sel_gui_copper_changes(c, ch, 64);
    CHECK(n == 11u);
    CHECK(ch[0].y == 4 && ch[0].pen == 255u && ch[0].rgb == 0x000000u);
    CHECK(ch[10].y == 14 && ch[10].rgb == 0xFF0A64u);
    for (i = 1; i < n; i++) CHECK(ch[i].y == ch[i - 1].y + 1 && ch[i].rgb >= ch[i - 1].rgb);
    CHECK(ch[5].rgb == 0x800532u || ch[5].rgb == 0x7F0532u); /* midpoint of 0..FF,0..0A,0..64 */
    /* the same gradient clipped in two pieces keeps one pen */
    sel_gui_clip_push(c, rc(0, 0, 10, 32));
    sel_gui_gradient(c, rc(0, 4, 64, 11), 1, 0xFF000000u, 0xFFFF0A64u);
    sel_gui_clip_pop(c);
    CHECK(c->copper_n == 1u);
    /* I628: a second, different gradient on disjoint rows rides the same pen (the copper reloads it); a flat gradient
     * yields one change */
    sel_gui_gradient(c, rc(0, 20, 64, 6), 1, 0xFF336699u, 0xFF336699u);
    n = sel_gui_copper_changes(c, ch, 64);
    CHECK(c->copper_n == 2u && px[20 * 64] == 255);
    CHECK(n == 12u && ch[11].y == 20 && ch[11].pen == 255u && ch[11].rgb == 0x336699u); /* 11 + 1 */
    /* I628: a gradient on rows another pen holds takes the next pen */
    sel_gui_gradient(c, rc(0, 10, 64, 4), 1, 0xFF000000u, 0xFF00FF00u);
    CHECK(c->copper_n == 3u && px[10 * 64] == 254);
    /* I628/I510: a gradient on exactly the rows of a recorded one shares its key: one line colour per row band */
    sel_gui_gradient(c, rc(0, 10, 64, 4), 1, 0xFF112233u, 0xFF445566u);
    CHECK(c->copper_n == 3u && px[10 * 64] == 254);
    /* a whole-canvas repaint hands the pens out again */
    sel_gui_dirty_add(c, rc(0, 0, 64, 32));
    CHECK(c->copper_n == 0u);
    free(c);
    free(px);
    /* pens off: nothing recorded, the dithered gradient is drawn */
    c = make(&px, 64, 32, 0u, 1);
    sel_gui_gradient(c, rc(0, 4, 64, 11), 1, 0xFF000000u, 0xFFFF0A64u);
    CHECK(c->copper_filled == 0u && sel_gui_copper_changes(c, ch, 64) == 0u && px[4 * 64] != 255);
    free(c);
    free(px);
    /* pens exhausted: a second gradient on overlapping rows is drawn dithered (and counted as a miss) */
    c = make(&px, 64, 32, 1u, 1);
    sel_gui_gradient(c, rc(0, 0, 64, 8), 1, 0xFF000000u, 0xFFFFFFFFu);
    sel_gui_gradient(c, rc(0, 4, 64, 8), 1, 0xFF000000u, 0xFF00FF00u);
    CHECK(c->copper_n == 1u && c->copper_filled == 8u && c->copper_miss == 1u);
    free(c);
    free(px);
    /* I468: ydiv 2 lays out in 64 rows, the screen has 32; the gradient rows are screen rows */
    c = make(&px, 64, 32, 4u, 2);
    sel_gui_gradient(c, rc(0, 8, 64, 16), 1, 0xFF000000u, 0xFFFFFFFFu);
    n = sel_gui_copper_changes(c, ch, 64);
    CHECK(c->copper_filled == 8u && n == 8u && ch[0].y == 4 && ch[7].y == 11 && ch[7].rgb == 0xFFFFFFu);
    free(c);
    free(px);
    /* I574: setting the 256-colour palette builds only the UI cube (32 entries x 32768 cells), not four region cubes
     * nobody draws into; a region cube is built on its first draw. A 68020 spent 118 s on the eager five-cube build. */
    c = make(&px, 64, 32, 4u, 1);
    CHECK(c->cube_ops == 32768u * 32u && c->rcube_stale == 0xFu);
    sel_gui_fill_rect(c, rc(0, 0, 8, 8), 0xFF1DB954u);
    CHECK(c->cube_ops == 32768u * 32u);
    sel_gui_set_region(c, SEL_GUI_RGN_ART);
    sel_gui_fill_rect(c, rc(0, 0, 8, 8), 0xFF1DB954u);
    CHECK(c->cube_ops == 32768u * 64u && c->rcube_stale == 0xEu);
    free(c);
    free(px);
    /* register words: pen 255 is COLOR31 of bank 7 */
    {
        uint32_t r[4][2];
        desk_aga_copper_regs(255u, 0xFF0A64u, r);
        CHECK(r[0][0] == 0x106u && r[0][1] == (0x0C20u | (7u << 13)));
        CHECK(r[1][0] == 0x180u + 62u && r[1][1] == 0xF06u);
        CHECK(r[2][1] == (r[0][1] | 0x200u));
        CHECK(r[3][0] == r[1][0] && r[3][1] == 0xFA4u);
    }
    /* the screen mode: native chipset IDs pass; RTG IDs fail, the Picasso96 mode of rig 2's Workbench (0x50041303) too,
     * which does not set DIPF_IS_FOREIGN and opened the AGA screen black (I441) */
    CHECK(desk_aga_mode_is_native(0x00029000u) == 1);   /* PAL HIRES */
    CHECK(desk_aga_mode_is_native(0x00019004u) == 1);   /* NTSC HIRES laced */
    CHECK(desk_aga_mode_is_native(0x00039025u) == 1);   /* VGA productivity */
    CHECK(desk_aga_mode_is_native(0x000A9004u) == 1);   /* DblPAL */
    CHECK(desk_aga_mode_is_native(0x00000000u) == 1);   /* default monitor LORES */
    CHECK(desk_aga_mode_is_native(0x50041303u) == 0);   /* Picasso96 (UAE RTG) */
    CHECK(desk_aga_mode_is_native(0x40D20001u) == 0);   /* CyberGraphX */
    CHECK(desk_aga_mode_is_native(0xFFFFFFFFu) == 0);   /* INVALID_ID */
    if (fails) return 1;
    printf("copper_gradient_unit: ok\n");
    return 0;
}
