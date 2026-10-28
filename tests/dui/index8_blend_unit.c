/* I595: INDEX8 anti-aliased edges blend against the colour the destination pixel really shows.
 * 1. A UI shape (rounded button) drawn over artwork: the edge blend maps through the cube of the art band the
 *    destination index belongs to, so every edge pixel is a real artwork pen (32..63), marked used in the art region,
 *    and the UI region claims no new pen for it.
 * 2. A translucent UI fill over a copper gradient: the destination colour is the row's copper colour
 *    (not the pen's single palette value), the result is a static entry (never a copper pen), and it differs per row. */
#include <selecta/gui/gui_draw.h>
#include <stdio.h>
#include <stdlib.h>

static int fails;
#define CHECK(c) \
    do { \
        if (!(c)) { \
            fprintf(stderr, "index8_blend_unit: line %d: %s\n", __LINE__, #c); \
            fails++; \
        } \
    } while (0)

static sel_gui_rect rc(int32_t x, int32_t y, int32_t w, int32_t h) {
    sel_gui_rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

/* UI entries 0..31: {0,128,255}^3 plus black repeats; art band 32..63: dark to light reds; the rest white. */
static sel_gui_canvas* make(uint8_t** px, int32_t w, int32_t h, uint32_t pens) {
    sel_gui_canvas* c = (sel_gui_canvas*)calloc(1, sizeof *c);
    uint32_t pal[256], i;
    static const uint32_t lv[3] = {0u, 128u, 255u};
    *px = (uint8_t*)calloc((size_t)w * (size_t)h, 1);
    sel_gui_canvas_init(c, SEL_GUI_INDEX8, w, h, *px);
    sel_gui_copper_enable(c, pens);
    for (i = 0; i < 256u; i++) {
        pal[i] = 0xFFFFFFu;
    }
    for (i = 0; i < 27u; i++) {
        pal[i] = (lv[i / 9u] << 16) | (lv[(i / 3u) % 3u] << 8) | lv[i % 3u];
    }
    for (i = 0; i < 32u; i++) {
        pal[32u + i] = ((0x30u + i * 6u) << 16) | (0x10u << 8) | 0x10u;
    }
    sel_gui_set_palette(c, pal, 0u, 256u);
    return c;
}

static int used_in(const sel_gui_canvas* c, uint32_t region, uint32_t idx) {
    return (c->used[region][idx >> 5] >> (idx & 31u)) & 1u;
}

static void edge_over_artwork(void) {
    uint8_t* px;
    sel_gui_canvas* c = make(&px, 32, 16, 0u);
    uint32_t used_ui_before, edge = 0u, x, y, solid;
    sel_gui_set_region(c, SEL_GUI_RGN_ART);
    sel_gui_fill_rect(c, rc(0, 0, 32, 16), 0xFFC03020u);
    CHECK(px[0] >= 32u && px[0] < 64u);
    sel_gui_set_region(c, SEL_GUI_RGN_UI);
    used_ui_before = 0u;
    for (x = 0; x < 8u; x++) {
        used_ui_before += (uint32_t)__builtin_popcount(c->used[SEL_GUI_RGN_UI][x]);
    }
    sel_gui_round_rect(c, rc(4, 2, 24, 12), 6, 0xFF2060FFu);
    solid = px[8 * 32 + 16];
    CHECK(solid < 32u); /* the button body is a UI entry */
    for (y = 0; y < 16u; y++) {
        for (x = 0; x < 32u; x++) {
            uint32_t v = px[y * 32u + x];
            if (v == solid || (v >= 32u && v < 64u && (x < 2u || x > 29u))) {
                continue;
            }
            if (v != px[0]) { /* neither the body nor untouched artwork: an edge pixel */
                edge++;
                CHECK(v >= 32u && v < 64u); /* picked from the art band, not the UI cube */
                CHECK(used_in(c, SEL_GUI_RGN_ART, v));
            }
        }
    }
    CHECK(edge > 4u);
    {
        uint32_t used_ui_after = 0u;
        for (x = 0; x < 8u; x++) {
            used_ui_after += (uint32_t)__builtin_popcount(c->used[SEL_GUI_RGN_UI][x]);
        }
        CHECK(used_ui_after == used_ui_before + 1u); /* only the button body colour */
    }
    free(c);
    free(px);
}

static void blend_over_copper(void) {
    uint8_t* px;
    sel_gui_canvas* c = make(&px, 32, 16, 4u);
    uint32_t top, bottom;
    sel_gui_gradient(c, rc(0, 0, 32, 16), 1, 0xFF000000u, 0xFFFF0000u);
    CHECK(c->copper_n == 1u && px[0] == 255u && px[15 * 32] == 255u);
    sel_gui_fill_rect(c, rc(4, 0, 8, 16), 0x800000FFu); /* half-transparent blue over black ... red */
    CHECK(px[4] < 252u && px[15 * 32 + 4] < 252u); /* a static entry, never a copper pen */
    top = sel_gui_pixel(c, 4, 0);
    bottom = sel_gui_pixel(c, 4, 15);
    CHECK(top == 0x000080u);    /* black + blue 50% */
    CHECK(bottom == 0x800080u); /* red + blue 50%: the row colour, not the pen's palette value */
    CHECK(px[4] != px[15 * 32 + 4]);
    free(c);
    free(px);
}

int main(void) {
    edge_over_artwork();
    blend_over_copper();
    if (fails != 0) {
        fprintf(stderr, "index8_blend_unit: %d failure(s)\n", fails);
        return 1;
    }
    return 0;
}
