/* mui-canvas-geom (MUI-P5-1), mui-grid (MUI-P5-2), mui-art-path (MUI-P5-4), plan 2026-10-25-amiga-mui.md.
 * Usage: mui_canvas_test geom|grid|art-path
 *
 * Reachability exemption (I691e, design thoughts/shared/research/2026-10-29-i691-platform-covers.md section 5): no live MUI
 * site calls mui_canvas_draw_grid or mui_canvas_draw_art today (mui_canvas_set_grid has no caller), so these cases drive the
 * platform cover through the canvas functions directly, not through the MUI top-level entry point. The MUI rig check happens
 * with the MUI row that wires a grid. */
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_genart.h>
#include <selecta/platform.h>
#include <selecta/uimodel/tokens.h>

#include <stdio.h>
#include <string.h>

#include "mui_canvas.h"

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

/* Fake class shim: what the Selecta_Canvas dispatcher does on MUIM_Show / MUIM_Draw / MUIM_Hide, with the object
 * rectangle a layout hands it. */
static int shim_show(mui_canvas* m, int32_t left, int32_t top, int32_t w, int32_t h) {
    return mui_canvas_fit(m, (sel_gui_rect){left, top, w, h});
}

static int test_geom(void) {
    mui_canvas m;
    const sel_gui_rect* clip;
    sel_gui_rect wc;
    memset(&m, 0, sizeof m);
    CHECK(shim_show(&m, 40, 30, 300, 200) == 1, "show");
    CHECK(m.c.w == 300 && m.c.h == 200, "canvas %dx%d, want 300x200", (int)m.c.w, (int)m.c.h);
    clip = &m.c.clip[0];
    CHECK(m.c.clip_n == 1u && clip->x == 0 && clip->y == 0 && clip->w == 300 && clip->h == 200, "clip %d,%d %dx%d",
          (int)clip->x, (int)clip->y, (int)clip->w, (int)clip->h);
    wc = mui_canvas_window_clip(&m);
    CHECK(wc.x == 40 && wc.y == 30 && wc.w == 300 && wc.h == 200, "window clip is the object rect");
    CHECK(shim_show(&m, 10, 10, 120, 90) == 1 && m.c.w == 120 && m.c.h == 90 && m.c.clip[0].w == 120 && m.c.clip[0].h == 90,
          "resize follows the object");
    CHECK(shim_show(&m, 0, 0, 0, 50) == 0 && m.pix == NULL, "empty object frees the canvas");
    mui_canvas_release(&m);
    return failures;
}

static const char* const g_titles[12] = {"Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel", "India", "Juliet", "Kilo", "Lima"};
static uint32_t g_art_calls;
static uint32_t g_cover[64 * 64];

static const uint32_t* art_cb(void* ctx, int64_t id) {
    (void)ctx;
    g_art_calls++;
    return id == 7 ? g_cover : NULL;
}

static int test_grid(void) {
    mui_grid_style s = {0xFF101010u, 0xFF303030u, 0xFFFFA000u, 64, 8, 8};
    mui_tile t[12];
    mui_canvas m;
    uint32_t i, drawn, f = 0;
    char hex[17];
    uint64_t h;
    memset(&m, 0, sizeof m);
    for (i = 0; i < 64u * 64u; i++) {
        g_cover[i] = 0xFF000000u | (i * 2654435761u >> 8);
    }
    for (i = 0; i < 12; i++) {
        t[i].title = g_titles[i];
        t[i].format = NULL;
        t[i].art_id = i == 2 ? 7 : 0; /* one tile with a cover through the art cache */
    }
    CHECK(mui_grid_cols(&s, 360) == 4u, "cols %u", (unsigned)mui_grid_cols(&s, 360));
    CHECK(mui_canvas_fit(&m, (sel_gui_rect){0, 0, 360, 272}) == 1, "fit");
    drawn = mui_canvas_draw_grid(&m, &s, t, 12, 5, art_cb, NULL);
    CHECK(drawn == 12u, "drew %u tiles, want 12", (unsigned)drawn);
    CHECK(m.draws == 1u && g_art_calls == 1u, "sentinel draws=%u art calls=%u", (unsigned)m.draws, (unsigned)g_art_calls);
    h = sel_gui_hash(&m.c);
    sel_gui_hash_hex(h, hex);
    CHECK(strcmp(hex, MUI_GRID_GOLDEN) == 0, "grid hash %s, golden %s", hex, MUI_GRID_GOLDEN);
    /* a different cursor changes the picture: the knob moves the output */
    mui_canvas_draw_grid(&m, &s, t, 12, 6, art_cb, NULL);
    sel_gui_hash_hex(sel_gui_hash(&m.c), hex);
    CHECK(strcmp(hex, MUI_GRID_GOLDEN) != 0, "cursor does not move the output");
    mui_canvas_release(&m);
    (void)f;
    return failures;
}

static int test_art_path(void) {
    mui_canvas m;
    uint32_t src[16 * 16];
    uint32_t tokens[SEL_GUI_TOKEN_COUNT];
    uint32_t i;
    memset(&m, 0, sizeof m);
    for (i = 0; i < 256; i++) {
        src[i] = 0xFF2040C0u;
    }
    mui_canvas_fit(&m, (sel_gui_rect){0, 0, 64, 64});
    CHECK(mui_art_path_for(1) == MUI_ART_DATATYPE, "RTG selects the datatype path");
    CHECK(mui_art_path_for(0) == MUI_ART_COVER, "planar selects generated art");
    CHECK(mui_canvas_draw_art(&m, (sel_gui_rect){0, 0, 32, 32}, mui_art_path_for(1), src, 16, 16, "mod", "x") == MUI_ART_DATATYPE &&
              sel_gui_pixel(&m.c, 10, 10) == 0x2040C0u,
          "RTG art blits the scaled cover");
    CHECK(mui_canvas_draw_art(&m, (sel_gui_rect){32, 32, 32, 32}, mui_art_path_for(0), src, 16, 16, "mod", "x") == MUI_ART_COVER &&
              sel_gui_pixel(&m.c, 42, 52) != 0x2040C0u,
          "planar ignores the cover and draws the platform cover");
    /* I691e, I691f: the field is a gradient ending in (bottom row)  the Amiga family tone the title "x" picks, in the default theme's tokens (not the grey placeholder). */
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        tokens[i] = sel_gui_token_default((sel_gui_token)i);
    }
    CHECK((sel_gui_pixel(&m.c, 34, 63) & 0xFFFFFFu) ==
              sel_gui_cover_tone(tokens, SEL_PLATFORM_FAMILY_AMIGA, sel_gui_cover_tone_of(sel_gui_str_hash("x"))),
          "the cover of \"mod\" is the Amiga tone, got %06x", (unsigned)(sel_gui_pixel(&m.c, 34, 63) & 0xFFFFFFu));
    mui_canvas_draw_art(&m, (sel_gui_rect){0, 0, 32, 32}, MUI_ART_COVER, NULL, 0, 0, "sid", "x");
    CHECK((sel_gui_pixel(&m.c, 2, 31) & 0xFFFFFFu) ==
              sel_gui_cover_tone(tokens, SEL_PLATFORM_FAMILY_COMMODORE_64, sel_gui_cover_tone_of(sel_gui_str_hash("x"))),
          "the cover of \"sid\" is the Commodore 64 tone");
    CHECK(mui_canvas_draw_art(&m, (sel_gui_rect){0, 0, 32, 32}, MUI_ART_DATATYPE, NULL, 0, 0, "mod", "x") == MUI_ART_COVER,
          "RTG without a decoded cover falls back to generated art");
    mui_canvas_release(&m);
    return failures;
}

int main(int argc, char** argv) {
    int r = 1;
    if (argc > 1 && strcmp(argv[1], "geom") == 0) {
        r = test_geom();
    } else if (argc > 1 && strcmp(argv[1], "grid") == 0) {
        r = test_grid();
    } else if (argc > 1 && strcmp(argv[1], "art-path") == 0) {
        r = test_art_path();
    }
    printf("%s\n", r == 0 ? "PASS" : "FAILED");
    return r == 0 ? 0 : 1;
}
