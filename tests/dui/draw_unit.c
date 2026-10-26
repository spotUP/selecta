/* Unit test of the integer rasterizer (DUI-P2-1) and its dirty list (DUI-P2-2): each primitive checked where it
 * lives, on both canvas formats. The wiring through the shell is proved by dui-draw's test card. */
#include <selecta/gui/gui_draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);                      \
            g_fail++;                                                                  \
        }                                                                              \
    } while (0)

static sel_gui_rect R(int32_t x, int32_t y, int32_t w, int32_t h) {
    sel_gui_rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

static sel_gui_canvas* make(uint32_t format, int32_t w, int32_t h) {
    sel_gui_canvas* c = (sel_gui_canvas*)malloc(sizeof *c);
    void* px = malloc(sel_gui_pixel_bytes(format, w, h));
    sel_gui_canvas_init(c, format, w, h, px);
    if (format == SEL_GUI_INDEX8) {
        uint32_t pal[256];
        sel_gui_palette_default(pal, 0x3B82F6u, 0x1E1E24u, 0xE6E6EBu);
        sel_gui_set_palette(c, pal, 0u, 256u);
    }
    return c;
}

static void drop(sel_gui_canvas* c) {
    free(c->pixels);
    free(c);
}

static uint32_t chan(uint32_t rgb, int shift) { return (rgb >> shift) & 255u; }

static void test_fill_and_clip(uint32_t fmt) {
    sel_gui_canvas* c = make(fmt, 32, 32);
    uint32_t n = 0u, bg = sel_gui_pixel(c, 31, 31);
    int32_t x, y;
    sel_gui_fill_rect(c, R(-5, -5, 10, 10), 0xFFFFFFFFu); /* 5x5 inside */
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            n += sel_gui_pixel(c, x, y) != bg;
        }
    }
    CHECK(n == 25u);
    CHECK(sel_gui_clip_push(c, R(10, 10, 4, 4)));
    sel_gui_fill_rect(c, R(0, 0, 32, 32), 0xFFFFFFFFu);
    sel_gui_clip_pop(c);
    CHECK(sel_gui_pixel(c, 10, 10) != bg && sel_gui_pixel(c, 13, 13) != bg);
    CHECK(sel_gui_pixel(c, 14, 14) == bg && sel_gui_pixel(c, 9, 10) == bg);
    sel_gui_fill_rect(c, R(20, 20, 2, 2), 0xFFFFFFFFu); /* clip is back to the whole canvas */
    CHECK(sel_gui_pixel(c, 21, 21) != bg);
    drop(c);
}

static void test_round_rect(void) {
    sel_gui_canvas* c = make(SEL_GUI_RGBA32, 40, 40);
    uint32_t partial = 0u;
    int32_t x, y;
    sel_gui_round_rect(c, R(4, 4, 32, 32), 8, 0xFFFF0000u);
    CHECK(sel_gui_pixel(c, 4, 4) == 0u);               /* the sharp corner stays empty */
    CHECK(sel_gui_pixel(c, 20, 20) == 0xFF0000u);      /* the middle is solid */
    CHECK(sel_gui_pixel(c, 20, 4) == 0xFF0000u);       /* straight edge is solid */
    for (y = 4; y < 12; y++) {
        for (x = 4; x < 12; x++) {
            uint32_t p = sel_gui_pixel(c, x, y);
            partial += p != 0u && p != 0xFF0000u;
        }
    }
    CHECK(partial > 4u);                                /* anti-aliased edge pixels exist on RGBA32 */
    drop(c);
    c = make(SEL_GUI_INDEX8, 40, 40);
    sel_gui_round_rect(c, R(4, 4, 32, 32), 8, 0xFFFF0000u);
    CHECK(sel_gui_pixel(c, 4, 4) == sel_gui_pixel(c, 0, 0));
    CHECK(sel_gui_pixel(c, 20, 20) != sel_gui_pixel(c, 0, 0));
    drop(c);
}

/* sel_gui_round_frame: a ring of the given width along the rounded edge, hollow inside, corners cut by the radius. */
static void test_round_frame(void) {
    sel_gui_canvas* c = make(SEL_GUI_RGBA32, 48, 48);
    int32_t i;
    sel_gui_round_frame(c, R(4, 4, 40, 40), 10, 3, 0xFF00FF00u);
    for (i = 14; i < 34; i++) { /* the straight edges: the three ring pixels are solid, the pixel just inside and outside are not */
        CHECK(sel_gui_pixel(c, i, 4) == 0x00FF00u && sel_gui_pixel(c, i, 6) == 0x00FF00u);
        CHECK(sel_gui_pixel(c, i, 41) == 0x00FF00u && sel_gui_pixel(c, i, 43) == 0x00FF00u);
        CHECK(sel_gui_pixel(c, 4, i) == 0x00FF00u && sel_gui_pixel(c, 6, i) == 0x00FF00u);
        CHECK(sel_gui_pixel(c, 41, i) == 0x00FF00u && sel_gui_pixel(c, 43, i) == 0x00FF00u);
        CHECK(sel_gui_pixel(c, i, 7) == 0u && sel_gui_pixel(c, i, 40) == 0u); /* inside the band edge */
        CHECK(sel_gui_pixel(c, i, 3) == 0u && sel_gui_pixel(c, i, 44) == 0u); /* outside the rect */
    }
    CHECK(sel_gui_pixel(c, 24, 24) == 0u);              /* the centre is untouched */
    CHECK(sel_gui_pixel(c, 12, 12) == 0u);              /* so is the inner corner area */
    CHECK(sel_gui_pixel(c, 4, 4) == 0u);                /* the sharp corner is cut by the radius */
    CHECK(sel_gui_pixel(c, 43, 4) == 0u && sel_gui_pixel(c, 4, 43) == 0u && sel_gui_pixel(c, 43, 43) == 0u);
    CHECK(sel_gui_pixel(c, 5, 5) == 0u);
    CHECK(sel_gui_pixel(c, 9, 5) != 0u);                /* the arc itself carries ring pixels near the corner */
    drop(c);
    c = make(SEL_GUI_RGBA32, 16, 16);
    sel_gui_round_frame(c, R(2, 2, 12, 12), 4, 0, 0xFF00FF00u); /* width 0 draws nothing */
    CHECK(sel_gui_pixel(c, 2, 8) == 0u && sel_gui_pixel(c, 8, 2) == 0u);
    drop(c);
}

static void test_gradient(void) {
    sel_gui_canvas* c = make(SEL_GUI_RGBA32, 64, 8);
    uint32_t mid;
    sel_gui_gradient(c, R(0, 0, 64, 8), 0, 0xFF000000u, 0xFFFFFFFFu);
    CHECK(sel_gui_pixel(c, 0, 0) == 0x000000u);
    CHECK(sel_gui_pixel(c, 63, 7) == 0xFFFFFFu);
    mid = chan(sel_gui_pixel(c, 32, 3), 16);
    CHECK(mid > 120u && mid < 136u);
    CHECK(chan(sel_gui_pixel(c, 10, 0), 16) < chan(sel_gui_pixel(c, 20, 0), 16));
    sel_gui_gradient(c, R(0, 0, 8, 8), 1, 0xFF0000FFu, 0xFFFF0000u);
    CHECK(sel_gui_pixel(c, 3, 0) == 0x0000FFu && sel_gui_pixel(c, 3, 7) == 0xFF0000u);
    drop(c);
    c = make(SEL_GUI_INDEX8, 64, 8); /* ordered dither: a mid grey mixes two palette entries along a row */
    sel_gui_gradient(c, R(0, 0, 64, 8), 0, 0xFF000000u, 0xFFFFFFFFu);
    CHECK(sel_gui_pixel(c, 0, 0) == 0x000000u);
    CHECK(sel_gui_pixel(c, 63, 7) == 0xFFFFFFu);
    drop(c);
}

static void test_line_mask_image(void) {
    sel_gui_canvas* c = make(SEL_GUI_RGBA32, 16, 16);
    uint8_t mask[4] = {255, 0, 128, 255};
    uint32_t img[4] = {0xFF000000u, 0xFFFFFFFFu, 0xFF000000u, 0xFFFFFFFFu}, n = 0u;
    int32_t i;
    sel_gui_line(c, 0, 0, 9, 9, 0xFFFFFFFFu);
    for (i = 0; i < 10; i++) {
        n += sel_gui_pixel(c, i, i) == 0xFFFFFFu;
    }
    CHECK(n == 10u && sel_gui_pixel(c, 1, 0) == 0u);
    sel_gui_blit_mask(c, 12, 0, mask, 2, 2, 2, 0xFF00FF00u);
    CHECK(sel_gui_pixel(c, 12, 0) == 0x00FF00u && sel_gui_pixel(c, 13, 0) == 0u);
    CHECK(chan(sel_gui_pixel(c, 12, 1), 8) > 120u && chan(sel_gui_pixel(c, 12, 1), 8) < 136u);
    sel_gui_blit_image(c, R(0, 12, 4, 2), img, 2, 2, 2, 0); /* nearest: x doubled */
    CHECK(sel_gui_pixel(c, 0, 12) == 0u && sel_gui_pixel(c, 3, 12) == 0xFFFFFFu);
    sel_gui_blit_image(c, R(8, 12, 1, 1), img, 2, 2, 2, 1); /* box: average of 4 source pixels */
    CHECK(chan(sel_gui_pixel(c, 8, 12), 16) >= 127u && chan(sel_gui_pixel(c, 8, 12), 16) <= 128u);
    sel_gui_blit_image(c, R(10, 12, 1, 1), img, 2, 2, 2, 0); /* nearest picks the first source pixel */
    CHECK(sel_gui_pixel(c, 10, 12) == 0u);
    drop(c);
}

static void test_scrim_shadow(void) {
    sel_gui_canvas* c = make(SEL_GUI_RGBA32, 64, 64);
    sel_gui_shadow s;
    uint32_t near_a, far_a;
    sel_gui_fill_rect(c, R(0, 0, 64, 64), 0xFFFFFFFFu);
    sel_gui_scrim(c, R(0, 0, 8, 8), 0x000000u, 128u);
    CHECK(chan(sel_gui_pixel(c, 2, 2), 16) >= 127u && chan(sel_gui_pixel(c, 2, 2), 16) <= 128u);
    CHECK(sel_gui_pixel(c, 20, 20) == 0xFFFFFFu);
    sel_gui_shadow_bake(&s, 8u, 0x000000u, 200u);
    sel_gui_shadow_draw(c, &s, R(24, 24, 16, 16));
    CHECK(sel_gui_pixel(c, 30, 30) == 0xFFFFFFu);      /* the box itself is untouched */
    near_a = 255u - chan(sel_gui_pixel(c, 30, 23), 16); /* just outside the top edge */
    far_a = 255u - chan(sel_gui_pixel(c, 30, 17), 16);  /* seven pixels out */
    CHECK(near_a > far_a && near_a > 20u);
    CHECK(sel_gui_pixel(c, 30, 10) == 0xFFFFFFu);       /* beyond the radius */
    drop(c);
}

static void test_cube(void) {
    sel_gui_canvas* c = (sel_gui_canvas*)malloc(sizeof *c);
    uint32_t pal[3] = {0x000000u, 0xFF0000u, 0x0000FFu};
    sel_gui_canvas_init(c, SEL_GUI_INDEX8, 4, 4, malloc(16));
    sel_gui_set_palette(c, pal, 0u, 3u);
    sel_gui_fill_rect(c, R(0, 0, 1, 1), 0xFFE02020u);
    sel_gui_fill_rect(c, R(1, 0, 1, 1), 0xFF2020D0u);
    sel_gui_fill_rect(c, R(2, 0, 1, 1), 0xFF101010u);
    CHECK(sel_gui_pixel(c, 0, 0) == 0xFF0000u);
    CHECK(sel_gui_pixel(c, 1, 0) == 0x0000FFu);
    CHECK(sel_gui_pixel(c, 2, 0) == 0x000000u);
    drop(c);
}

static void test_dirty(void) {
    sel_gui_canvas* c = make(SEL_GUI_RGBA32, 256, 256);
    int32_t i;
    uint32_t n, total = 0u;
    sel_gui_dirty_add(c, R(0, 0, 10, 10));
    sel_gui_dirty_add(c, R(20, 20, 10, 10));
    CHECK(c->dirty_n == 2u && sel_gui_dirty_pixels(c) == 200u);
    sel_gui_dirty_add(c, R(2, 2, 3, 3)); /* contained: the list does not grow */
    CHECK(c->dirty_n == 2u && sel_gui_dirty_pixels(c) == 200u);
    sel_gui_dirty_add(c, R(5, 5, 10, 10)); /* overlaps the first: merged into its bounding box 0,0 15x15 */
    CHECK(c->dirty_n == 2u && sel_gui_dirty_pixels(c) == 225u + 100u);
    sel_gui_dirty_add(c, R(-20, -20, 10, 10)); /* wholly outside the canvas: ignored */
    CHECK(c->dirty_n == 2u);
    n = sel_gui_present(c);
    CHECK(n == 325u && c->dirty_n == 0u && sel_gui_dirty_pixels(c) == 0u);
    /* A hundred scattered 2x2 rectangles overflow the 64-entry list: it merges, never drops, never exceeds. */
    for (i = 0; i < 100; i++) {
        sel_gui_dirty_add(c, R((i % 10) * 24, (i / 10) * 24, 2, 2));
        CHECK(c->dirty_n <= SEL_GUI_DIRTY_MAX);
    }
    CHECK(c->dirty_n == SEL_GUI_DIRTY_MAX || c->dirty_n > 32u);
    {
        uint32_t k;
        int32_t x, y, covered = 0;
        for (i = 0; i < 100; i++) {
            int32_t rx = (i % 10) * 24, ry = (i / 10) * 24;
            int ok = 0;
            for (k = 0; k < c->dirty_n; k++) {
                sel_gui_rect d = c->dirty[k];
                if (rx >= d.x && ry >= d.y && rx + 2 <= d.x + d.w && ry + 2 <= d.y + d.h) {
                    ok = 1;
                }
            }
            covered += ok;
        }
        CHECK(covered == 100);
        for (k = 0; k < c->dirty_n; k++) { /* the list stays disjoint */
            uint32_t m;
            for (m = k + 1u; m < c->dirty_n; m++) {
                CHECK(!sel_gui_rect_intersects(c->dirty[k], c->dirty[m]));
            }
        }
        (void)x;
        (void)y;
    }
    total = sel_gui_present(c);
    CHECK(total >= 400u && total <= 256u * 256u);
    drop(c);
}

/* The published FNV-1a 64 vectors, and the hex form the goldens use. */
static void test_fnv64(void) {
    char hex[17];
    sel_gui_hash_hex(sel_gui_fnv64(SEL_GUI_FNV64_BASIS, "", 0u), hex);
    CHECK(strcmp(hex, "cbf29ce484222325") == 0);
    sel_gui_hash_hex(sel_gui_fnv64(SEL_GUI_FNV64_BASIS, "a", 1u), hex);
    CHECK(strcmp(hex, "af63dc4c8601ec8c") == 0);
    sel_gui_hash_hex(sel_gui_fnv64(SEL_GUI_FNV64_BASIS, "foobar", 6u), hex);
    CHECK(strcmp(hex, "85944171f73967e8") == 0);
    CHECK(sel_gui_fnv64_word(SEL_GUI_FNV64_BASIS, 0x64636261u) == sel_gui_fnv64(SEL_GUI_FNV64_BASIS, "abcd", 4u));
}

/* I375: antialiased text on INDEX8. Edge coverage must blend to intermediate palette entries, not threshold to 2. */
static void test_index8_text_blend(void) {
    sel_gui_canvas* c = make(SEL_GUI_INDEX8, 16, 4);
    uint8_t mask[8] = {0, 32, 64, 96, 128, 160, 192, 255};
    uint32_t seen[8], n = 0u, i, j;
    sel_gui_fill_rect(c, R(0, 0, 16, 4), 0xFF1E1E24u);
    sel_gui_blit_mask(c, 0, 0, mask, 8, 1, 8, 0xFFE6E6EBu);
    for (i = 0u; i < 8u; i++) {
        uint32_t px = sel_gui_pixel(c, (int32_t)i, 0), dup = 0u;
        for (j = 0u; j < n; j++) {
            dup |= seen[j] == px;
        }
        if (!dup) {
            seen[n++] = px;
        }
    }
    CHECK(n > 2u);
    drop(c);
}

int main(void) {
    test_fnv64();
    test_fill_and_clip(SEL_GUI_RGBA32);
    test_fill_and_clip(SEL_GUI_INDEX8);
    test_round_rect();
    test_round_frame();
    test_gradient();
    test_line_mask_image();
    test_scrim_shadow();
    test_index8_text_blend();
    test_cube();
    test_dirty();
    if (g_fail != 0) {
        printf("draw_unit: %d checks failed\n", g_fail);
        return 1;
    }
    printf("draw_unit: ok\n");
    return 0;
}
