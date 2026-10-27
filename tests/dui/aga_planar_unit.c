/* Unit test of the aga backend's pure helpers (I439, M4-P6-1): dirty rectangles to row bands, the reference
 * chunky-to-planar conversion and its inverse, the chip RAM budget and the LoadRGB32 table. The Amiga build runs the
 * same conversion through Kalms' c2p1x1_8_c5_040 on 040/060; this reference is the 020/030 path and the golden the
 * rig frame dump is compared against (M4-P6-2). */
#include "amiga/aga_planar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;

#define CHECK(c) \
    do { \
        if (!(c)) { \
            fprintf(stderr, "aga_planar_unit: line %d: %s\n", __LINE__, #c); \
            fails++; \
        } \
    } while (0)

static uint32_t rng = 0x1234567u;
static uint8_t next_byte(void) {
    rng = rng * 1103515245u + 12345u;
    return (uint8_t)(rng >> 16);
}

static void test_bit_layout(void) {
    /* 8x1 screen: pixel 0 = 0x01 sets bit 7 of plane 0, pixel 7 = 0x80 sets bit 0 of plane 7. */
    uint8_t chunky[8] = {0x01, 0, 0, 0, 0, 0, 0, 0x80};
    uint8_t planes[8];
    memset(planes, 0xee, sizeof planes);
    desk_aga_c2p_ref(chunky, 8, 0, 1, planes, 1);
    CHECK(planes[0] == 0x80);
    CHECK(planes[7] == 0x01);
    for (int p = 1; p < 7; p++) {
        CHECK(planes[p] == 0x00);
    }
}

static void test_round_trip_and_bands(void) {
    enum { W = 640, H = 64 };
    uint32_t bplsize = W / 8 * H;
    uint8_t* chunky = malloc(W * H);
    uint8_t* planes = malloc(bplsize * 8);
    uint8_t* back = malloc(W * H);
    for (int i = 0; i < W * H; i++) {
        chunky[i] = next_byte();
    }
    memset(planes, 0, bplsize * 8);
    desk_aga_c2p_ref(chunky, W, 0, H, planes, bplsize);
    desk_aga_p2c_ref(planes, bplsize, W, H, back);
    CHECK(memcmp(chunky, back, W * H) == 0);

    /* A band conversion writes only its rows: change rows 10-19, convert that band, the rest of the planes keep
     * the old frame and the whole screen equals the new chunky buffer. */
    uint8_t* before = malloc(bplsize * 8);
    memcpy(before, planes, bplsize * 8);
    for (int i = 10 * W; i < 20 * W; i++) {
        chunky[i] = (uint8_t)~chunky[i];
    }
    desk_aga_c2p_ref(chunky, W, 10, 10, planes, bplsize);
    desk_aga_p2c_ref(planes, bplsize, W, H, back);
    CHECK(memcmp(chunky, back, W * H) == 0);
    int untouched = 1;
    for (int p = 0; p < 8; p++) {
        const uint8_t* a = before + p * bplsize;
        const uint8_t* b = planes + p * bplsize;
        if (memcmp(a, b, 10 * W / 8) != 0 || memcmp(a + 20 * W / 8, b + 20 * W / 8, (H - 20) * W / 8) != 0) {
            untouched = 0;
        }
    }
    CHECK(untouched);
    free(before);
    free(chunky);
    free(planes);
    free(back);
}

static void test_bands(void) {
    desk_aga_band b[DESK_AGA_BANDS_MAX];
    sel_gui_rect r[4] = {{0, 100, 20, 10}, {300, 5, 10, 10}, {10, 108, 50, 4}, {0, 500, 640, 40}};
    uint32_t n = desk_aga_bands(r, 4, 512, 0, b, DESK_AGA_BANDS_MAX);
    CHECK(n == 3);
    CHECK(b[0].y == 5 && b[0].h == 10);
    CHECK(b[1].y == 100 && b[1].h == 12);
    CHECK(b[2].y == 500 && b[2].h == 12); /* clipped to the 512-row screen */
    CHECK(desk_aga_band_rows(b, n) == 34);

    /* gap joins: rows 5-14 and 100-111 are 85 rows apart */
    n = desk_aga_bands(r, 4, 512, 85, b, DESK_AGA_BANDS_MAX);
    CHECK(n == 2 && b[0].y == 5 && b[0].h == 107);

    /* max: three bands into two joins the closest pair (5-14 with 100-111) */
    n = desk_aga_bands(r, 4, 512, 0, b, 2);
    CHECK(n == 2 && b[0].y == 5 && b[0].h == 107 && b[1].y == 500);

    /* empty and off-screen rectangles give no band */
    sel_gui_rect e[2] = {{0, 0, 0, 10}, {0, 600, 10, 10}};
    CHECK(desk_aga_bands(e, 2, 512, 0, b, DESK_AGA_BANDS_MAX) == 0);

    /* union with the previous frame's bands (double buffer) */
    desk_aga_band now[DESK_AGA_BANDS_MAX] = {{50, 10}};
    desk_aga_band prev[2] = {{0, 4}, {55, 20}};
    n = desk_aga_bands_union(now, 1, prev, 2, 0, DESK_AGA_BANDS_MAX);
    CHECK(n == 2 && now[0].y == 0 && now[0].h == 4 && now[1].y == 50 && now[1].h == 25);
}

static void test_budget_and_palette(void) {
    CHECK(desk_aga_buffer_bytes(640, 512) == 327680u);
    CHECK(desk_aga_chip_bytes(640, 512) == 655360u); /* 640 KB, M4 plan 4.3 */
    CHECK(desk_aga_chip_bytes(640, 256) == 327680u); /* 320 KB */
    uint32_t rgb[2] = {0x123456u, 0xff0080u};
    uint32_t t[2 * 3 + 2];
    desk_aga_rgb32_table(rgb, 32, 2, t);
    CHECK(t[0] == ((2u << 16) | 32u));
    CHECK(t[1] == 0x12121212u && t[2] == 0x34343434u && t[3] == 0x56565656u);
    CHECK(t[4] == 0xffffffffu && t[5] == 0x00000000u && t[6] == 0x80808080u);
    CHECK(t[7] == 0);
}

/* I441: the AGA screen size never comes from Workbench: 640x256 by default, 320x256 on request, nothing else. */
static void test_pick_size(void) {
    int32_t w = 0, h = 0;
    CHECK(desk_aga_pick_size(&w, &h, 0, 0, 0) == 1 && w == 640 && h == 256);
    w = 0;
    h = 0;
    CHECK(desk_aga_pick_size(&w, &h, 1, 0, 0) == 1 && w == 640 && h == 200);
    w = 320;
    h = 256;
    CHECK(desk_aga_pick_size(&w, &h, 0, 0, 0) == 0 && w == 320 && h == 256);
    w = 800;
    h = 600;
    CHECK(desk_aga_pick_size(&w, &h, 0, 0, 0) == -1); /* the RTG Workbench size of rig 2 */
    w = 640;
    h = 512;
    CHECK(desk_aga_pick_size(&w, &h, 0, 0, 0) == -1); /* no interlace */
    w = 1280;
    h = 800;
    CHECK(desk_aga_pick_size(&w, &h, 0, 0, 0) == -1); /* the sdl default */
}

/* I566: the blue border. The desktop palette holds the accent blue in pen 0, which is COLOR00, which is what the border
 * shows. Every copper write sets BRDRBLNK (BPLCON3 bit 5) so the border is black. */
static void test_border_blank(void) {
    uint32_t r[4][2];
    desk_aga_copper_regs(253u, 0x123456u, r);
    CHECK(r[0][0] == 0x106u && (r[0][1] & 0x20u) != 0u);
    CHECK(r[2][0] == 0x106u && (r[2][1] & 0x20u) != 0u);
    CHECK((DESK_AGA_BPLCON3 & 0x20u) != 0u);
}

/* I566: the setting ui.desktop.overscan. Off (the default) keeps 640x256 / 640x200 whatever the mode offers; on takes the fit.
 * Both request the border blank through VideoControl so graphics.library sets ECSENA with BRDRBLNK. */
static void test_overscan_setting(void) {
    int32_t w = 0, h = 0, mw = 724, mh = 283;
    uint32_t vt[4];
    desk_aga_overscan_area(0, &mw, &mh);
    CHECK(desk_aga_pick_size(&w, &h, 0, mw, mh) == 1 && w == 640 && h == 256);
    mw = 724;
    mh = 283;
    w = 0;
    h = 0;
    desk_aga_overscan_area(1, &mw, &mh);
    CHECK(desk_aga_pick_size(&w, &h, 0, mw, mh) == 1 && w == 704 && h == 280);
    mw = 724;
    mh = 241;
    w = 0;
    h = 0;
    desk_aga_overscan_area(0, &mw, &mh);
    CHECK(desk_aga_pick_size(&w, &h, 1, mw, mh) == 1 && w == 640 && h == 200);
    CHECK(desk_aga_border_blank_tags(vt) == 3 && vt[0] == 0x80000005u && vt[2] == 0u);
}

/* I566: the app fills the visible display. rig 2 (PAL): QueryOverscan(OSCAN_MAX) = -72,-15 to 651,267 = 724 x 283. */
static void test_overscan_fit(void) {
    int32_t w = 0, h = 0, rect[4] = {-72, -15, 651, 267}, clip[4];
    CHECK(desk_aga_pick_size(&w, &h, 0, 724, 283) == 1 && w == 704 && h == 280);
    w = 640;
    h = 256;
    CHECK(desk_aga_pick_size(&w, &h, 0, 724, 283) == 1); /* the standard size stays valid */
    w = 704;
    h = 280;
    CHECK(desk_aga_pick_size(&w, &h, 0, 724, 283) == 1);
    w = 0;
    h = 0;
    CHECK(desk_aga_pick_size(&w, &h, 0, 0, 0) == 1 && w == 640 && h == 256); /* no overscan info */
    w = 0;
    h = 0;
    CHECK(desk_aga_pick_size(&w, &h, 1, 724, 241) == 1 && w == 704 && h == 240); /* NTSC */
    desk_aga_fit_clip(rect, 704, 280, clip);
    CHECK(clip[0] == -62 && clip[2] == 641 && clip[1] == -14 && clip[3] == 265);
}

/* I574: a 68020 (rig 2, the A1200) gets Kalms' 030 c2p; the C reference there took 16 s per 640x256 frame and froze
 * the pointer. 68040/060 keep the 040 routine; only a 68000/010 falls back to the reference. */
static void test_c2p_pick(void) {
    const uint32_t lo = 320u / 8u * 200u; /* 8000 bytes a plane: inside the 030 routine's 16 KB */
    CHECK(desk_aga_c2p_pick(1u << 1, lo) == 2u);                         /* 68020 */
    CHECK(desk_aga_c2p_pick((1u << 1) | (1u << 2), lo) == 2u);           /* 68030 */
    CHECK(desk_aga_c2p_pick((1u << 1) | (1u << 2) | (1u << 3), lo) == 1u); /* 68040 */
    CHECK(desk_aga_c2p_pick((1u << 3) | (1u << 7), lo) == 1u);           /* 68060 */
    CHECK(desk_aga_c2p_pick(0u, lo) == 0u);                              /* 68000 */
    CHECK(desk_aga_c2p_pick(1u << 1, 16384u) == 2u);
}

/* I580: the 030 routine's 16-bit plane displacements wrap above 16 KB a plane. The 640x256 desktop (20480 bytes a
 * plane, Johan's flat blue background on rig 2) must get the 040 routine on a 68020/030. */
static void test_c2p_pick_large_planes(void) {
    const uint32_t big = 640u / 8u * 256u;
    CHECK(desk_aga_c2p_pick(1u << 1, big) == 1u);                 /* 68020 */
    CHECK(desk_aga_c2p_pick((1u << 1) | (1u << 2), big) == 1u);   /* 68030 */
    CHECK(desk_aga_c2p_pick(1u << 1, 640u / 8u * 200u) == 2u);    /* 640x200: 16000 bytes still fits */
    CHECK(desk_aga_c2p_pick(1u << 1, 16385u) == 1u);
}

/* I566: QueryOverscan is an intuition.library call. desk_aga_default_size opened graphics.library only, so with
 * ui.desktop.overscan=1 the call went through a NULL IntuitionBase and froze the whole Amiga (rig 2). The backend is
 * Amiga-only, so this reads its source: every function that reaches QueryOverscan (max_overscan) must run after
 * intuition.library is opened. */
static void test_query_overscan_has_intuition(void) {
    FILE* f = fopen(SELECTA_AGA_BACKEND_SRC, "rb");
    static char src[40000];
    size_t n, cap;
    char *def, *end, *mo, *cl;
    CHECK(f != NULL);
    if (f == NULL) return;
    n = fread(src, 1, sizeof src - 1, f);
    fclose(f);
    src[n] = 0;
    cap = n;
    (void)cap;
    def = strstr(src, "void desk_aga_default_size(");
    CHECK(def != NULL);
    if (def == NULL) return;
    end = strstr(def, "\nstatic int fail(");
    CHECK(end != NULL);
    if (end == NULL) return;
    *end = 0;
    mo = strstr(def, "max_overscan(");
    cl = strstr(def, "\"intuition.library\"");
    CHECK(mo != NULL && cl != NULL && cl < mo);
}

int main(void) {
    test_query_overscan_has_intuition();
    test_c2p_pick();
    test_c2p_pick_large_planes();
    test_pick_size();
    test_border_blank();
    test_overscan_fit();
    test_overscan_setting();
    test_bit_layout();
    test_round_trip_and_bands();
    test_bands();
    test_budget_and_palette();
    if (fails) {
        fprintf(stderr, "aga_planar_unit: %d failures\n", fails);
        return 1;
    }
    printf("aga_planar_unit: ok\n");
    return 0;
}
