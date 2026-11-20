/* mui-canvas-geom (MUI-P5-1), mui-grid (MUI-P5-2), mui-art-path (MUI-P5-4), plan 2026-10-25-amiga-mui.md.
 * Usage: mui_canvas_test geom|grid|tiles|art-path
 *
 * Reachability exemption (I691e, design thoughts/shared/research/2026-10-29-i691-platform-covers.md section 5): no live MUI
 * site calls mui_canvas_draw_grid or mui_canvas_draw_art today (mui_canvas_set_grid has no caller), so these cases drive the
 * platform cover through the canvas functions directly, not through the MUI top-level entry point. The MUI rig check happens
 * with the MUI row that wires a grid. */
#include <selecta/gui/gui_draw.h>
#include <selecta/library_item.h>
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
        t[i].kind = 0u; /* FORMAT; left uninitialised, the hash followed stack garbage and moved per build */
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

static int test_tiles(void) {
    static sel_view_row rows[4];
    sel_view_state v;
    mui_tile t[4];
    mui_grid_style a = mui_grid_style_for(640, 1, 2, 3), b = mui_grid_style_for(1024, 1, 2, 3);
    memset(rows, 0, sizeof rows);
    memset(&v, 0, sizeof v);
    rows[0].kind = SEL_ROW_HEADING; rows[0].text = "Head";
    rows[1].kind = SEL_ROW_FILE; rows[1].text = "Song"; rows[1].artist = "Zed"; rows[1].format = "mod"; rows[1].art_id = 9;
    rows[2].kind = SEL_ROW_HOME; rows[2].text = "Album"; rows[2].album = 1; rows[2].key = "Album"; rows[2].item_kind = 1u + (uint32_t)SEL_LIB_ITEM_ALBUM;
    v.rows = rows; v.row_n = 3;
    CHECK(mui_grid_tiles(NULL, t, 4) == 0u && mui_grid_tiles(&v, t, 4) == 2u, "heading skipped");
    CHECK(strcmp(t[0].title, "Song") == 0 && strcmp(t[0].artist, "Zed") == 0 && strcmp(t[0].format, "mod") == 0 && t[0].art_id == 9, "song tile fields");
    CHECK(t[0].kind == sel_cover_kind_of_item(0, 0, 0, NULL) && t[1].artist == NULL && t[1].kind == sel_cover_kind_of_item(rows[2].item_kind, 1, 0, "Album"), "kinds");
    CHECK(mui_grid_tiles(&v, t, 1) == 1u, "cap");
    CHECK(a.edge == 96 && b.edge == 128 && a.tile == 2u && b.selected == 3u, "style edge by screen width");
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
    CHECK(mui_canvas_draw_art(&m, (sel_gui_rect){0, 0, 32, 32}, mui_art_path_for(1), src, 16, 16, 0u, "mod", "x") == MUI_ART_DATATYPE &&
              sel_gui_pixel(&m.c, 10, 10) == 0x2040C0u,
          "RTG art blits the scaled cover");
    CHECK(mui_canvas_draw_art(&m, (sel_gui_rect){32, 32, 32, 32}, mui_art_path_for(0), src, 16, 16, 0u, "mod", "x") == MUI_ART_COVER &&
              sel_gui_pixel(&m.c, 42, 52) != 0x2040C0u,
          "planar ignores the cover and draws the platform cover");
    /* I691e, I691f: the field is a gradient ending in (bottom row)  the Amiga family tone the title "x" picks, in the default theme's tokens (not the grey placeholder). */
    for (i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        tokens[i] = sel_gui_token_default((sel_gui_token)i);
    }
    CHECK((sel_gui_pixel(&m.c, 34, 63) & 0xFFFFFFu) ==
              sel_gui_cover_tone(tokens, SEL_PLATFORM_FAMILY_AMIGA, sel_gui_cover_tone_of(sel_gui_str_hash("x"))),
          "the cover of \"mod\" is the Amiga tone, got %06x", (unsigned)(sel_gui_pixel(&m.c, 34, 63) & 0xFFFFFFu));
    mui_canvas_draw_art(&m, (sel_gui_rect){0, 0, 32, 32}, MUI_ART_COVER, NULL, 0, 0, 0u, "sid", "x");
    CHECK((sel_gui_pixel(&m.c, 2, 31) & 0xFFFFFFu) ==
              sel_gui_cover_tone(tokens, SEL_PLATFORM_FAMILY_COMMODORE_64, sel_gui_cover_tone_of(sel_gui_str_hash("x"))),
          "the cover of \"sid\" is the Commodore 64 tone");
    /* I1399a: a synthetic icon set (every mask solid) stands in for the generated baked table the desktop plugin owns. */
    {
        static uint8_t stream[SEL_GUI_ICON_COUNT * 8u];
        static uint32_t off[SEL_GUI_ICON_COUNT + 1u];
        static const uint32_t* offs[1] = {off};
        static const uint8_t* streams[1] = {stream};
        static const int32_t sizes[1] = {8};
        static sel_gui_icons_packed pk;
        uint8_t solid[64];
        uint32_t k, at = 0u;

        memset(solid, 255, sizeof solid);
        for (k = 0u; k < SEL_GUI_ICON_COUNT; k++) {
            off[k] = at;
            at += (uint32_t)sel_gui_icon_pack(solid, sizeof solid, stream + at);
        }
        off[SEL_GUI_ICON_COUNT] = at;
        pk.count = SEL_GUI_ICON_COUNT;
        pk.size_n = 1u;
        pk.sizes = sizes;
        pk.stream = streams;
        pk.off = offs;
        mui_canvas_set_icons(&pk);
    }
    /* I1399a: a playlist kind covers in its kind tint with the icon, not the family tone. */
    mui_canvas_draw_art(&m, (sel_gui_rect){0, 0, 64, 64}, MUI_ART_COVER, NULL, 0, 0, SEL_COVER_KIND_PLAYLIST, "mod", "x");
    CHECK((sel_gui_pixel(&m.c, 2, 63) & 0xFFFFFFu) != sel_gui_cover_tone(tokens, SEL_PLATFORM_FAMILY_AMIGA, sel_gui_cover_tone_of(sel_gui_str_hash("x"))),
          "a kind cover does not use the family tone");
    CHECK((sel_gui_pixel(&m.c, 32, 32) & 0xFFFFFFu) != (sel_gui_pixel(&m.c, 2, 32) & 0xFFFFFFu), "the kind icon is drawn in the middle");
    CHECK(mui_canvas_draw_art(&m, (sel_gui_rect){0, 0, 32, 32}, MUI_ART_DATATYPE, NULL, 0, 0, 0u, "mod", "x") == MUI_ART_COVER,
          "RTG without a decoded cover falls back to generated art");
    mui_canvas_set_icons(NULL);
    mui_canvas_release(&m);
    return failures;
}

int main(int argc, char** argv) {
    int r = 1;
    if (argc > 1 && strcmp(argv[1], "geom") == 0) {
        r = test_geom();
    } else if (argc > 1 && strcmp(argv[1], "grid") == 0) {
        r = test_grid();
    } else if (argc > 1 && strcmp(argv[1], "tiles") == 0) {
        r = test_tiles();
    } else if (argc > 1 && strcmp(argv[1], "art-path") == 0) {
        r = test_art_path();
    }
    printf("%s\n", r == 0 ? "PASS" : "FAILED");
    return r == 0 ? 0 : 1;
}
