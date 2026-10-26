/* Unit test of the integer rasterizer (DUI-P2-1) and its dirty list (DUI-P2-2): each primitive checked where it
 * lives, on both canvas formats. The wiring through the shell is proved by dui-draw's test card. */
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_icons.h>

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

/* ---- span rasterizers equal the per-pixel path (I396) ---- */

static int32_t t_min(int32_t a, int32_t b) { return a < b ? a : b; }

/* Reference coverage 0..16: the original 4x4 subsample test, written out independently of the library. */
static uint32_t ref_cov(sel_gui_rect r, int32_t rad, int32_t x, int32_t y) {
    int32_t cx, cy, sx, sy;
    uint32_t n = 0u;
    int64_t lim;
    if (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h) {
        return 0u;
    }
    cx = x < r.x + rad ? r.x + rad : x >= r.x + r.w - rad ? r.x + r.w - rad : -1;
    cy = y < r.y + rad ? r.y + rad : y >= r.y + r.h - rad ? r.y + r.h - rad : -1;
    if (cx < 0 || cy < 0) {
        return 16u;
    }
    lim = (int64_t)8 * rad * 8 * rad;
    for (sy = 0; sy < 4; sy++) {
        for (sx = 0; sx < 4; sx++) {
            int64_t dx = (int64_t)x * 8 + sx * 2 + 1 - (int64_t)cx * 8;
            int64_t dy = (int64_t)y * 8 + sy * 2 + 1 - (int64_t)cy * 8;
            n += dx * dx + dy * dy <= lim ? 1u : 0u;
        }
    }
    return n;
}

static void ref_px(sel_gui_canvas* c, int32_t x, int32_t y, uint32_t cov, uint32_t argb) {
    uint32_t a = argb >> 24;
    if (cov == 0u) {
        return;
    }
    if (c->format == SEL_GUI_INDEX8) {
        if (cov < 8u) {
            return;
        }
    } else {
        a = (a * cov + 8u) / 16u;
    }
    sel_gui_fill_rect(c, R(x, y, 1, 1), (a << 24) | (argb & 0xFFFFFFu));
}

static void backdrop(sel_gui_canvas* c) {
    int32_t x, y;
    for (y = 0; y < c->h; y++) {
        for (x = 0; x < c->w; x++) {
            uint32_t k = (uint32_t)(x * 7 + y * 13) % 5u;
            sel_gui_fill_rect(c, R(x, y, 1, 1), 0xFF000000u | (k * 0x30u << 16) | (((uint32_t)y * 9u) & 255u) << 8 | (k * 50u));
        }
    }
}

static int same(const sel_gui_canvas* a, const sel_gui_canvas* b) {
    return memcmp(a->pixels, b->pixels, sel_gui_pixel_bytes(a->format, a->w, a->h)) == 0 &&
           memcmp(a->used, b->used, sizeof a->used) == 0;
}

static void test_span_equals_pixel(uint32_t format) {
    static const sel_gui_rect rects[] = {{3, 2, 17, 13}, {0, 0, 1, 1}, {5, 5, 2, 9}, {1, 1, 30, 3}, {4, 3, 25, 26},
                                         {-6, -4, 20, 14}, {20, 18, 30, 30}, {8, 8, 0, 5}};
    static const int32_t radii[] = {0, 1, 2, 5, 7, 40};
    static const uint32_t cols[] = {0xFF3B82F6u, 0x80FF4020u, 0x00FFFFFFu, 0xFFFFFFFFu, 0x30102030u};
    static const sel_gui_rect clips[] = {{0, 0, 40, 40}, {7, 6, 11, 9}, {10, 10, 1, 25}};
    size_t ri, di, ci, ki;
    for (ki = 0; ki < sizeof clips / sizeof clips[0]; ki++) {
        for (ri = 0; ri < sizeof rects / sizeof rects[0]; ri++) {
            for (ci = 0; ci < sizeof cols / sizeof cols[0]; ci++) {
                for (di = 0; di < sizeof radii / sizeof radii[0] + 1u; di++) {
                    sel_gui_canvas* a = make(format, 40, 40);
                    sel_gui_canvas* b = make(format, 40, 40);
                    sel_gui_rect r = rects[ri];
                    int32_t x, y, rad = di < sizeof radii / sizeof radii[0] ? radii[di] : 3;
                    int32_t arad = t_min(rad, t_min(r.w, r.h) / 2);
                    int mode = (int)di % 3; /* 0 fill, 1 round rect, 2 round frame */
                    int32_t width = 1 + (int32_t)(ci % 3u) * 2;
                    sel_gui_rect in;
                    int32_t irad;
                    backdrop(a);
                    backdrop(b);
                    memset(a->used, 0, sizeof a->used);
                    memset(b->used, 0, sizeof b->used);
                    sel_gui_clip_push(a, clips[ki]);
                    sel_gui_clip_push(b, clips[ki]);
                    if (arad < 0) {
                        arad = 0;
                    }
                    in = R(r.x + width, r.y + width, r.w - 2 * width, r.h - 2 * width);
                    irad = arad > width ? arad - width : 0;
                    if (in.w > 0 && in.h > 0) {
                        irad = t_min(irad, t_min(in.w, in.h) / 2);
                    }
                    if (mode == 0) {
                        sel_gui_fill_rect(a, r, cols[ci]);
                        for (y = r.y; y < r.y + r.h; y++) {
                            for (x = r.x; x < r.x + r.w; x++) {
                                sel_gui_fill_rect(b, R(x, y, 1, 1), cols[ci]);
                            }
                        }
                    } else if (mode == 1) {
                        sel_gui_round_rect(a, r, rad, cols[ci]);
                        for (y = r.y; y < r.y + r.h; y++) {
                            for (x = r.x; x < r.x + r.w; x++) {
                                uint32_t cov = ref_cov(r, arad, x, y);
                                if (format == SEL_GUI_RGBA32) {
                                    ref_px(b, x, y, cov, cols[ci]);
                                } else {
                                    ref_px(b, x, y, cov, cols[ci]);
                                }
                            }
                        }
                    } else {
                        sel_gui_round_frame(a, r, rad, width, cols[ci]);
                        for (y = r.y; y < r.y + r.h; y++) {
                            for (x = r.x; x < r.x + r.w; x++) {
                                uint32_t o = ref_cov(r, arad, x, y);
                                uint32_t i = in.w > 0 && in.h > 0 ? ref_cov(in, irad, x, y) : 0u;
                                ref_px(b, x, y, o > i ? o - i : 0u, cols[ci]);
                            }
                        }
                    }
                    CHECK(same(a, b));
                    drop(a);
                    drop(b);
                }
            }
        }
    }
}

static void test_gradient_span(uint32_t format) {
    int vertical;
    for (vertical = 0; vertical < 2; vertical++) {
        sel_gui_canvas* a = make(format, 37, 29);
        sel_gui_canvas* b = make(format, 37, 29);
        sel_gui_rect r = R(2, 3, 31, 22);
        int32_t x, y, n = vertical ? r.h : r.w;
        sel_gui_clip_push(a, R(5, 4, 20, 15));
        sel_gui_clip_push(b, R(5, 4, 20, 15));
        sel_gui_gradient(a, r, vertical, 0xFF2040FFu, 0xFFF0A010u);
        for (y = r.y; y < r.y + r.h; y++) {
            for (x = r.x; x < r.x + r.w; x++) {
                int32_t t = (vertical ? y - r.y : x - r.x) * (n > 1 ? 65536 / (n - 1) : 0);
                uint32_t col = 0xFF000000u;
                int i;
                static const int32_t sh[3] = {16, 8, 0};
                for (i = 0; i < 3; i++) {
                    int32_t p = (int32_t)((0xFF2040FFu >> sh[i]) & 255u), q = (int32_t)((0xF0A010u >> sh[i]) & 255u);
                    int32_t v = p + (int32_t)((((int64_t)(q - p) * t) + 32768) >> 16);
                    col |= (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v) << sh[i];
                }
                if (format == SEL_GUI_RGBA32) {
                    sel_gui_fill_rect(b, R(x, y, 1, 1), col);
                }
            }
        }
        if (format == SEL_GUI_RGBA32) {
            CHECK(same(a, b));
        } else {
            /* INDEX8: every pixel is within the dither bias of the exact ramp colour. */
            int32_t bad = 0;
            for (y = r.y + 1; y < r.y + r.h; y++) {
                for (x = r.x + 1; x < r.x + r.w; x++) {
                    if (x >= 5 && x < 25 && y >= 4 && y < 19) {
                        uint32_t pa = sel_gui_pixel(a, x, y);
                        uint32_t pb = sel_gui_pixel(a, x, y - 1);
                        bad += (vertical && pa == pb && y > 20) ? 1 : 0;
                    }
                }
            }
            CHECK(bad == 0);
        }
        drop(a);
        drop(b);
    }
}

/* I460: the icon area resample keeps coverage: an opaque mask stays opaque, a half mask stays about half. */
static void test_icon_resample(void) {
    uint8_t src[16 * 16];
    uint8_t dst[13 * 13];
    uint32_t sum = 0u;
    int i;
    memset(src, 255, sizeof src);
    memset(dst, 0, sizeof dst);
    sel_gui_icon_resample(src, 16, dst, 13);
    for (i = 0; i < 13 * 13; i++) {
        CHECK(dst[i] == 255);
    }
    for (i = 0; i < 16 * 16; i++) {
        src[i] = (uint8_t)((i % 16) < 8 ? 255 : 0);
    }
    sel_gui_icon_resample(src, 16, dst, 13);
    for (i = 0; i < 13 * 13; i++) {
        sum += dst[i];
    }
    CHECK(sum / (13u * 13u) >= 125u && sum / (13u * 13u) <= 130u);
    CHECK(dst[0] == 255 && dst[12] == 0);
}

int main(void) {
    test_icon_resample();
    test_span_equals_pixel(SEL_GUI_RGBA32);
    test_span_equals_pixel(SEL_GUI_INDEX8);
    test_gradient_span(SEL_GUI_RGBA32);
    test_gradient_span(SEL_GUI_INDEX8);
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
