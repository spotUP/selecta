/* mui-canvas-geom (MUI-P5-1), mui-grid (MUI-P5-2), mui-art-path (MUI-P5-4), plan 2026-10-25-amiga-mui.md.
 * Usage: mui_canvas_test geom|grid|grid-aga|tiles|art-path|art
 *
 * Reachability exemption (I691e, design thoughts/shared/research/2026-10-29-i691-platform-covers.md section 5): no live MUI
 * site calls mui_canvas_draw_grid or mui_canvas_draw_art today (mui_canvas_set_grid has no caller), so these cases drive the
 * platform cover through the canvas functions directly, not through the MUI top-level entry point. The MUI rig check happens
 * with the MUI row that wires a grid. */
#include <selecta/gui/gui_draw.h>
#include <selecta/library_item.h>
#include <selecta/gui/gui_genart.h>
#include <selecta/gui/gui_image.h>
#include <selecta/platform.h>
#include <selecta/uimodel/tokens.h>

#include <stdio.h>
#include <string.h>

#include "mui_canvas.h"
#include "mui_grid_art.h"

#include <stdlib.h>

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
    mui_grid_style s = {0xFF101010u, 0xFF303030u, 0xFFFFA000u, 64, 8, 8, 24, 0u};
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
        t[i].artist = "Artist";
        t[i].format = NULL;
        t[i].kind = 0u; /* FORMAT; left uninitialised, the hash followed stack garbage and moved per build */
        t[i].art_id = i == 5 ? 7 : 0; /* one tile with a cover through the art cache */
    }
    CHECK(mui_grid_cols(&s, 360) == 4u, "cols %u", (unsigned)mui_grid_cols(&s, 360));
    CHECK(mui_canvas_fit(&m, (sel_gui_rect){0, 0, 360, 272}) == 1, "fit");
    /* tile height 80 + 24 caption: two rows fit 272, cursor 9 is on row 2, so the first row drawn is 1 and tiles 4..11 show */
    CHECK(mui_grid_visible_rows(&s, 272) == 2u && mui_grid_first_row(&s, 360, 272, 9) == 1u && mui_grid_first_row(&s, 360, 272, 5) == 0u,
          "visible rows %u first row %u", (unsigned)mui_grid_visible_rows(&s, 272), (unsigned)mui_grid_first_row(&s, 360, 272, 9));
    CHECK(mui_grid_tile_rect(&s, 360, 4).h == 104, "tile height includes the caption");
    drawn = mui_canvas_draw_grid(&m, &s, t, 12, 9, 1u, art_cb, NULL);
    CHECK(drawn == 8u, "drew %u tiles, want 8", (unsigned)drawn);
    CHECK(m.draws == 1u && g_art_calls == 1u, "sentinel draws=%u art calls=%u", (unsigned)m.draws, (unsigned)g_art_calls);
    h = sel_gui_hash(&m.c);
    sel_gui_hash_hex(h, hex);
    CHECK(strcmp(hex, MUI_GRID_GOLDEN) == 0, "grid hash %s, golden %s", hex, MUI_GRID_GOLDEN);
    /* a different cursor changes the picture: the knob moves the output */
    mui_canvas_draw_grid(&m, &s, t, 12, 10, 1u, art_cb, NULL);
    sel_gui_hash_hex(sel_gui_hash(&m.c), hex);
    CHECK(strcmp(hex, MUI_GRID_GOLDEN) != 0, "cursor does not move the output");
    {
        uint32_t a = mui_grid_signature(t, 12, 9), b;
        CHECK(a == mui_grid_signature(t, 12, 9) && a != mui_grid_signature(t, 12, 10) && a != mui_grid_signature(t, 11, 9), "signature");
        t[3].artist = "Other";
        b = mui_grid_signature(t, 12, 9);
        CHECK(a != b, "signature follows the artist text");
    }
    mui_canvas_release(&m);
    (void)f;
    return failures;
}

/* I1473g5 mui-grid-aga: the planar compose into a byte canvas. Cover bytes are cube indices 0..26, background 27, card 28, outline 29. */
typedef struct aga_t {
    uint8_t* buf;
    int32_t w, h;
    uint32_t tints, covers;
} aga_t;

static void aga_t_rect(void* ctx, sel_gui_rect r, uint32_t rgb) {
    aga_t* a = ctx;
    uint8_t v = rgb == 0x101010u ? 27u : rgb == 0x303030u ? 28u : 29u;
    int32_t x, y;
    for (y = r.y < 0 ? 0 : r.y; y < r.y + r.h && y < a->h; y++) {
        for (x = r.x < 0 ? 0 : r.x; x < r.x + r.w && x < a->w; x++) {
            a->buf[y * a->w + x] = v;
        }
    }
}

static void aga_t_cover(void* ctx, sel_gui_rect r, const uint8_t* idx, uint32_t tint) {
    aga_t* a = ctx;
    int32_t x, y;
    if (idx == NULL) {
        a->tints++;
        (void)tint;
        return;
    }
    a->covers++;
    for (y = 0; y < r.h && r.y + y < a->h; y++) {
        for (x = 0; x < r.w && r.x + x < a->w; x++) {
            a->buf[(r.y + y) * a->w + r.x + x] = idx[y * r.w + x];
        }
    }
}

static uint32_t g_aga_words[32 * 32];

static const uint32_t* aga_art(void* ctx, int64_t id) {
    int32_t i;
    (void)ctx;
    for (i = 0; i < 32 * 32; i++) {
        g_aga_words[i] = 0xFF000000u | ((uint32_t)i * 2654435761u >> 8) * (uint32_t)id;
    }
    return g_aga_words;
}

static int test_grid_aga(void) {
    mui_grid_style s = {0xFF101010u, 0xFF303030u, 0xFFFFA000u, 32, 4, 4, 12, 0u};
    static mui_tile t[12];
    static uint8_t buf[200 * 160];
    aga_t a = {buf, 200, 160, 0, 0};
    mui_aga_sink sink = {aga_t_rect, aga_t_cover, NULL};
    mui_aga_cache* c = mui_aga_cache_open(32, 4);
    uint32_t i, drawn, max = 0, h = 2166136261u;
    char hex[17];
    for (i = 0; i < 12; i++) {
        t[i].title = "T";
        t[i].artist = "A";
        t[i].format = NULL;
        t[i].kind = 0u;
        t[i].art_id = i == 3 ? 0 : (int64_t)i + 1; /* tile 3 has no cover: the tint path */
    }
    CHECK(c != NULL && mui_aga_cache_open(0, 4) == NULL, "cache open");
    CHECK(mui_tile_tint(&t[0]) <= 0xFFFFFFu, "tint is rgb");
    drawn = mui_canvas_draw_grid_aga(&s, 200, 160, t, 12, 5, 0u, aga_art, NULL, c, &sink, &a);
    CHECK(drawn == 12u && a.tints == 1u && a.covers == 11u, "drew %u tiles, tints %u covers %u", (unsigned)drawn, (unsigned)a.tints, (unsigned)a.covers);
    for (i = 0; i < sizeof buf; i++) {
        max = buf[i] > max ? buf[i] : max;
        h = (h ^ buf[i]) * 16777619u;
    }
    CHECK(max <= 29u, "pens above 27 + 3: %u", (unsigned)max);
    snprintf(hex, sizeof hex, "%08x", (unsigned)h);
    CHECK(strcmp(hex, MUI_GRID_AGA_GOLDEN) == 0, "aga hash %s, golden %s", hex, MUI_GRID_AGA_GOLDEN);
    mui_aga_cache_close(c);
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

/* I1473g7: keys, wheel, hit test and tile-to-row mapping of the tile canvases (Grid, later Cover Flow). */
static int test_input(void) {
    mui_grid_input in;
    mui_grid_style st = mui_grid_style_for(640, 1, 2, 3);
    static sel_view_row rows[4];
    sel_view_state v;
    int32_t tw, th, x, y;
    uint32_t cols;
    st.caption = 30;
    tw = st.edge + 2 * st.pad;
    th = tw + st.caption;
    cols = mui_grid_cols(&st, 600);
    CHECK(cols >= 3u, "cols %u", (unsigned)cols);
    CHECK(mui_grid_key(MUI_RAWKEY_LEFT, cols, &in) && in.kind == MUI_GIN_MOVE && in.arg == -1, "left");
    CHECK(mui_grid_key(MUI_RAWKEY_RIGHT, cols, &in) && in.arg == 1, "right");
    CHECK(mui_grid_key(MUI_RAWKEY_UP, cols, &in) && in.arg == -(int32_t)cols, "up is a row");
    CHECK(mui_grid_key(MUI_RAWKEY_DOWN, cols, &in) && in.arg == (int32_t)cols, "down is a row");
    CHECK(mui_grid_key(MUI_RAWKEY_WHEEL_DOWN, cols, &in) && in.arg == (int32_t)cols, "wheel down one row");
    CHECK(mui_grid_key(MUI_RAWKEY_WHEEL_UP, cols, &in) && in.arg == -(int32_t)cols, "wheel up one row");
    CHECK(mui_grid_key(MUI_RAWKEY_RETURN, cols, &in) && in.kind == MUI_GIN_ACTIVATE, "return");
    CHECK(mui_grid_key(MUI_RAWKEY_ENTER, cols, &in) && in.kind == MUI_GIN_ACTIVATE, "enter");
    CHECK(!mui_grid_key(0x20u, cols, &in) && in.kind == MUI_GIN_NONE, "other key left alone");
    /* Cover Flow: row_step 0 = left/right and the wheel step one cover, up/down are not ours */
    CHECK(mui_grid_key(MUI_RAWKEY_LEFT, 0u, &in) && in.arg == -1 && mui_grid_key(MUI_RAWKEY_WHEEL_DOWN, 0u, &in) && in.arg == 1, "flow steps one");
    CHECK(!mui_grid_key(MUI_RAWKEY_UP, 0u, &in) && !mui_grid_key(MUI_RAWKEY_DOWN, 0u, &in), "flow ignores up/down");
    /* hit: tile (col 1, row 0) and (col 0, row 1), the gap, the margin, past the last tile, a scrolled grid */
    x = st.gap + (tw + st.gap) + 2;
    y = st.gap + 2;
    CHECK(mui_grid_hit(&st, 600, 400, 20u, 0u, x, y) == 1, "hit col 1");
    CHECK(mui_grid_hit(&st, 600, 400, 20u, 0u, st.gap + 2, st.gap + th + st.gap + 2) == (int32_t)cols, "hit row 1");
    CHECK(mui_grid_hit(&st, 600, 400, 20u, 0u, st.gap + tw + 1, y) == -1, "gap");
    CHECK(mui_grid_hit(&st, 600, 400, 20u, 0u, 1, 1) == -1, "margin");
    CHECK(mui_grid_hit(&st, 600, 400, 2u, 0u, x + (tw + st.gap), y) == -1, "past the last tile");
    CHECK(mui_grid_hit(&st, 600, 400, 40u, 39u, st.gap + 2, y) == (int32_t)(mui_grid_first_row(&st, 600, 400, 39u) * cols), "scrolled: first drawn tile");
    /* tile index to window row skips headings */
    memset(rows, 0, sizeof rows);
    memset(&v, 0, sizeof v);
    rows[0].kind = SEL_ROW_HEADING;
    rows[2].kind = SEL_ROW_HEADING;
    v.rows = rows;
    v.row_n = 4;
    CHECK(mui_grid_tile_row(&v, 0) == 1 && mui_grid_tile_row(&v, 1) == 3 && mui_grid_tile_row(&v, 2) == -1 && mui_grid_tile_row(NULL, 0) == -1, "tile row");
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

/* I1473g3 mui-grid-art: an in-memory library with album 5 (thumbnail row), 6 (cover only, wide), 7 (nothing). */
static uint8_t g_thumb5[SEL_GUI_THUMB_HEAD + 4u * 144u * 144u], g_thumb6[SEL_GUI_THUMB_HEAD + 4u * 144u * 144u];
static uint32_t g_thumb5_len, g_thumb6_len, g_puts, g_decodes;

static uint32_t ga_get(void* ctx, int64_t album, const char* kind, uint8_t* out, uint32_t cap) {
    const uint8_t* b = NULL;
    uint32_t n = 0;
    (void)ctx;
    if (strcmp(kind, "thumbf144") == 0) {
        b = album == 5 ? g_thumb5 : g_thumb6;
        n = album == 5 ? g_thumb5_len : album == 6 ? g_thumb6_len : 0u;
    } else if (strcmp(kind, "cover") == 0 && album == 6) {
        n = 4u; /* a stand-in blob: the fake decoder ignores it */
    }
    if (out != NULL && n > 0u && n <= cap && b != NULL) {
        memcpy(out, b, n);
    }
    return n;
}

static int ga_put(void* ctx, int64_t album, const char* kind, const uint8_t* data, uint32_t len) {
    (void)ctx;
    g_puts++;
    if (album == 6 && strcmp(kind, "thumbf144") == 0 && len <= sizeof g_thumb6) {
        memcpy(g_thumb6, data, len);
        g_thumb6_len = len;
        return 1;
    }
    return 0;
}

static uint32_t* ga_decode(const sel_ui_art* art, int64_t album, uint32_t frame, int32_t* w, int32_t* h) {
    uint32_t* p;
    uint32_t i;
    (void)art;
    (void)frame;
    g_decodes++;
    if (album != 6) {
        return NULL;
    }
    *w = 200;
    *h = 100;
    p = (uint32_t*)malloc(200u * 100u * sizeof *p);
    for (i = 0; i < 200u * 100u; i++) {
        p[i] = 0xFF2040C0u;
    }
    return p;
}

static int test_grid_art(void) {
    sel_ui_art art = {NULL, NULL, ga_get, ga_put, NULL};
    mui_tile t[4];
    static uint32_t px[144 * 144];
    uint32_t i;
    const uint32_t* a;
    mui_grid_art* g;
    memset(t, 0, sizeof t);
    t[0].art_id = 0;
    t[1].art_id = 7;
    t[2].art_id = 5;
    t[3].art_id = 6;
    for (i = 0; i < 144u * 144u; i++) {
        px[i] = 0xFF00FF00u;
    }
    g_thumb5_len = sel_gui_thumb_pack(px, 144, 144, g_thumb5, sizeof g_thumb5);
    CHECK(g_thumb5_len > 0u, "pack");
    CHECK(mui_grid_art_open(NULL, ga_decode, 96, 4) == NULL && mui_grid_art_open(&art, ga_decode, 200, 4) == NULL, "bad open");
    g = mui_grid_art_open(&art, ga_decode, 96, 4);
    CHECK(g != NULL, "open");
    CHECK(mui_grid_art_get(g, 5) == NULL, "empty cache misses");
    CHECK(mui_grid_art_tick(g, t, 4) == 0 && g_decodes == 1u, "album 7 has no cover: one miss, nothing loaded (decodes %u)", (unsigned)g_decodes);
    CHECK(mui_grid_art_tick(g, t, 4) == 5 && g_decodes == 1u && g_puts == 0u, "thumbnail row served without decode or write");
    a = mui_grid_art_get(g, 5);
    CHECK(a != NULL && a[0] == 0xFF00FF00u && a[96 * 96 - 1] == 0xFF00FF00u && mui_grid_art_get(g, 6) == NULL, "tile 96 squared from the thumbnail, one per tick");
    CHECK(mui_grid_art_tick(g, t, 4) == 6 && g_decodes == 2u && g_puts == 1u && mui_grid_art_decodes(g) == 1u, "decode on miss writes the thumbnail back");
    a = mui_grid_art_get(g, 6);
    CHECK(a != NULL && a[0] == 0xFF000000u && a[96 * 48] == 0xFF2040C0u, "wide cover is fitted with black bars (bar %08x, middle %08x)",
          a ? (unsigned)a[0] : 0u, a ? (unsigned)a[96 * 48] : 0u);
    CHECK(mui_grid_art_tick(g, t, 4) == 0 && g_decodes == 2u, "bad album 7 is not retried, nothing left");
    mui_grid_art_close(g);
    g = mui_grid_art_open(&art, ga_decode, 96, 4);
    CHECK(mui_grid_art_tick(g, t + 3, 1) == 6 && g_decodes == 2u && g_puts == 1u, "a second loader reads the written-back thumbnail");
    mui_grid_art_close(g);
    return failures;
}

static int test_flow(void) {
    mui_grid_style s = {0xFF101010u, 0xFF303030u, 0xFFFFA000u, 64, 8, 8, 24, 0xFF000000u};
    mui_tile t[7];
    mui_canvas m;
    mui_flow_view rest, glide, ref;
    uint32_t i, drawn;
    char hex[17], hex2[17];
    memset(&m, 0, sizeof m);
    memset(t, 0, sizeof t);
    for (i = 0; i < 64u * 64u; i++) {
        g_cover[i] = 0xFF000000u | (i * 2654435761u >> 8);
    }
    for (i = 0; i < 7; i++) {
        t[i].title = g_titles[i];
        t[i].artist = "Artist";
        t[i].art_id = i == 3 ? 7 : 0; /* the front cover has art (upscaled), the rest the platform cover */
    }
    CHECK(mui_canvas_fit(&m, (sel_gui_rect){0, 0, 480, 320}) == 1, "fit");
    drawn = mui_canvas_draw_flow(&m, &s, t, 7, 3 << 16, art_cb, NULL, &rest);
    mui_flow_layout(&s, 480, 320, 3 << 16, 7, &ref);
    CHECK(drawn == 7u && rest.n == 7u && m.draws == 1u, "drew %u of 7, draws %u", (unsigned)drawn, (unsigned)m.draws);
    CHECK(memcmp(rest.cv, ref.cv, sizeof rest.cv) == 0 && rest.big == ref.big, "draw layout equals mui_flow_layout");
    CHECK(rest.cv[6].g == 3u && rest.cv[6].sz == rest.big && rest.cv[6].alpha == 0, "front cover last, full size, no scrim");
    CHECK(rest.cv[0].alpha == 150 && rest.cv[0].sz >= rest.big * 4 / 10 && rest.cv[0].sz <= rest.big * 4 / 10 + 1, "far cover scrim %d size %d (big %d)", (int)rest.cv[0].alpha, (int)rest.cv[0].sz, (int)rest.big);
    CHECK(rest.caption.x == rest.cv[6].x && rest.caption.w == rest.big && rest.caption.y == rest.cv[6].y + rest.big + 2 && rest.caption.h == 24,
          "caption sits under the front cover: %d,%d %dx%d", (int)rest.caption.x, (int)rest.caption.y, (int)rest.caption.w, (int)rest.caption.h);
    CHECK(rest.cv[6].x + rest.big / 2 == 240, "front cover centred");
    sel_gui_hash_hex(sel_gui_hash(&m.c), hex);
    mui_canvas_draw_flow(&m, &s, t, 7, 3 << 16, art_cb, NULL, NULL);
    sel_gui_hash_hex(sel_gui_hash(&m.c), hex2);
    CHECK(strcmp(hex, hex2) == 0 && strcmp(hex, MUI_FLOW_GOLDEN) == 0, "rest hash %s, again %s, golden %s", hex, hex2, MUI_FLOW_GOLDEN);
    mui_canvas_draw_flow(&m, &s, t, 7, (3 << 16) + 32768, art_cb, NULL, &glide);
    sel_gui_hash_hex(sel_gui_hash(&m.c), hex2);
    CHECK(strcmp(hex2, hex) != 0 && glide.n == 7u && strcmp(hex2, MUI_FLOW_GLIDE_GOLDEN) == 0, "glide hash %s, golden %s", hex2, MUI_FLOW_GLIDE_GOLDEN);
    CHECK(glide.cv[glide.n - 1u].x != rest.cv[6].x || glide.cv[glide.n - 1u].g != rest.cv[6].g, "mid-glide front cover moved");
    CHECK(mui_flow_glide_q16(0) == 0 && mui_flow_glide_q16(220) == 65536 && mui_flow_glide_q16(110) > 32768 && mui_flow_glide_q16(110) < 65536, "ease out: 110 ms is %d", (int)mui_flow_glide_q16(110));
    CHECK(mui_flow_glides(1) == 1 && mui_flow_glides(0) == 0, "glide on 040/060 only (D-F2)");
    mui_canvas_release(&m);
    return failures;
}

/* I1473g8b3 mui-flow-aga: strips into a byte canvas (background 27); records every strip. */
typedef struct fa_t {
    uint8_t* buf;
    int32_t w, h;
    uint32_t strips, tints;
    int64_t pixels;
    int bad;
} fa_t;

static void fa_rect(void* ctx, sel_gui_rect r, uint32_t rgb) {
    fa_t* a = ctx;
    (void)r;
    (void)rgb;
    memset(a->buf, 27, (size_t)a->w * (size_t)a->h);
}

static void fa_cover(void* ctx, sel_gui_rect r, const uint8_t* idx, uint32_t tint) {
    fa_t* a = ctx;
    (void)r;
    (void)tint;
    a->tints += idx == NULL;
}

static void fa_chunky(void* ctx, sel_gui_rect r, const uint8_t* idx) {
    fa_t* a = ctx;
    int32_t x, y;
    a->strips++;
    a->pixels += (int64_t)r.w * r.h;
    for (y = 0; y < r.h; y++) {
        for (x = 0; x < r.w; x++) {
            uint8_t v = idx[y * r.w + x];
            a->bad |= v >= 27u || r.x + x < 0 || r.x + x >= a->w || r.y + y < 0 || r.y + y >= a->h;
            if (!a->bad) {
                a->buf[(r.y + y) * a->w + r.x + x] = v;
            }
        }
    }
}

static int test_flow_aga(void) {
    mui_grid_style s = {0xFF101010u, 0xFF303030u, 0xFFFFA000u, 32, 8, 8, 24, 0xFF000000u};
    static mui_tile t[7];
    static uint8_t buf[480 * 320];
    fa_t a = {buf, 480, 320, 0, 0, 0, 0};
    mui_aga_sink sink = {fa_rect, fa_cover, fa_chunky};
    mui_aga_cache* c = mui_aga_cache_open(32, 12);
    mui_flow_view v, ref;
    uint8_t l0[27], l90[27], l150[27];
    uint32_t i, drawn, h = 2166136261u;
    char hex[17];
    for (i = 0; i < 7; i++) {
        t[i].title = "T";
        t[i].artist = "A";
        t[i].art_id = i == 1 ? 0 : (int64_t)i + 1; /* cover 1 has no art: the dimmed tint */
    }
    drawn = mui_canvas_draw_flow_aga(&s, 480, 320, t, 7, 3 << 16, aga_art, NULL, c, &sink, &a, &v);
    mui_flow_layout(&s, 480, 320, 3 << 16, 7, &ref);
    CHECK(drawn == 7u && memcmp(v.cv, ref.cv, sizeof v.cv) == 0, "layout equals mui_flow_layout, drew %u", (unsigned)drawn);
    CHECK(a.strips == 6u && a.tints == 1u && !a.bad, "strips %u (want 6), tints %u, bad %d", (unsigned)a.strips, (unsigned)a.tints, a.bad);
    CHECK(a.pixels * 10 <= (int64_t)v.big * v.big * 16, "pixels %lld exceed 1.6 big^2 (big %d)", (long long)a.pixels, (int)v.big);
    mui_cover_dim_lut(0, 0u, l0);
    mui_cover_dim_lut(90, 0u, l90);
    mui_cover_dim_lut(150, 0u, l150);
    CHECK(l0[26] == 26 && l90[26] == 13 && l150[26] == 13 && l0[0] == 0 && l150[13] == 0, "dim: white at 0 -> %d, 90 -> %d, 150 -> %d, mid grey 150 -> %d", l0[26], l90[26], l150[26], l150[13]);
    mui_canvas_draw_flow_aga(&s, 480, 320, t, 7, (3 << 16) + 32768, aga_art, NULL, c, &sink, &a, NULL);
    for (i = 0; i < sizeof buf; i++) {
        h = (h ^ buf[i]) * 16777619u;
    }
    snprintf(hex, sizeof hex, "%08x", (unsigned)h);
    CHECK(strcmp(hex, MUI_FLOW_AGA_GOLDEN) == 0, "flow-aga hash %s, golden %s", hex, MUI_FLOW_AGA_GOLDEN);
    mui_aga_cache_close(c);
    return failures;
}

int main(int argc, char** argv) {
    int r = 1;
    if (argc > 1 && strcmp(argv[1], "geom") == 0) {
        r = test_geom();
    } else if (argc > 1 && strcmp(argv[1], "grid") == 0) {
        r = test_grid();
    } else if (argc > 1 && strcmp(argv[1], "grid-aga") == 0) {
        r = test_grid_aga();
    } else if (argc > 1 && strcmp(argv[1], "input") == 0) {
        r = test_input();
    } else if (argc > 1 && strcmp(argv[1], "tiles") == 0) {
        r = test_tiles();
    } else if (argc > 1 && strcmp(argv[1], "art-path") == 0) {
        r = test_art_path();
    } else if (argc > 1 && strcmp(argv[1], "flow") == 0) {
        r = test_flow();
    } else if (argc > 1 && strcmp(argv[1], "flow-aga") == 0) {
        r = test_flow_aga();
    } else if (argc > 1 && strcmp(argv[1], "art") == 0) {
        r = test_grid_art();
    }
    printf("%s\n", r == 0 ? "PASS" : "FAILED");
    return r == 0 ? 0 : 1;
}
