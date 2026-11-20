/* Canvas of the MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P5-1, MUI-P5-2, MUI-P5-4): the pure part of the Selecta_Canvas
 * class. The class (mui_canvas_class.c, Amiga only) owns one of these per object; everything it decides lives here so
 * host tests reach it: canvas geometry, the tile grid drawn with src/gui/draw, and the artwork path per screen kind. */
#ifndef MUI_CANVAS_H
#define MUI_CANVAS_H

#include <selecta/engine.h>
#include <selecta/gui/gui_draw.h>
#include <selecta/gui/gui_icons.h>

#include <stdint.h>

typedef struct mui_canvas {
    sel_gui_canvas c; /* RGBA32 canvas the size of the object (MUI-P5-1) */
    uint32_t* pix;    /* owned pixel buffer, w * h words */
    sel_gui_rect obj; /* the object rectangle in window coordinates: where the class blits the canvas */
    uint32_t draws;   /* grid frames drawn: the sentinel that the drawing code ran */
} mui_canvas;

/* MUIM_Show / MUIM_Draw: (re)creates the canvas for the object rectangle `obj` (window coordinates, the object's
 * _mleft/_mtop/_mwidth/_mheight). The canvas is obj.w x obj.h and its clip is the whole canvas, which is the object
 * rectangle moved to the origin. Returns 0 on empty size or out of memory. A same-size call keeps the buffer. */
int mui_canvas_fit(mui_canvas* m, sel_gui_rect obj);
/* MUIM_Hide / MUIM_Cleanup. */
void mui_canvas_release(mui_canvas* m);
/* The clip the class installs on the window RastPort before the blit: the object rectangle in window coordinates. */
sel_gui_rect mui_canvas_window_clip(const mui_canvas* m);

/* One grid tile: the title seeds the cover tone, art_id keys the art cache (0 = no cover). */
typedef struct mui_tile {
    const char* title;
    const char* artist; /* caption line under the title, NULL when unknown (D3: title and artist, as the Mac) */
    const char* format; /* file extension; its platform family colours the cover (NULL = Music) */
    int64_t art_id;
    uint32_t kind; /* SEL_COVER_KIND_* (include/selecta/platform.h, I1399a); 0 = FORMAT: the family of `format` */
} mui_tile;

/* Cover lookup: tile-edge square words of the cover with `art_id`, NULL on a miss (the tile then shows the platform cover).
 * Backed by sel_gui_art_cache_get in the class (MUI-P5-2). */
typedef const uint32_t* (*mui_art_fn)(void* ctx, int64_t art_id);

typedef struct mui_grid_style {
    uint32_t background, tile, selected; /* 0xAARRGGBB, filled from MUI pens by the class */
    int32_t edge, gap, pad;              /* cover edge, gap between tiles, padding inside a tile */
    int32_t caption; /* I1473g4: height of the title + artist block under the cover (2 font lines + spacing, set by the class from the
                      * window font); 0 = no captions, tile height = tile width as before */
    uint32_t scrim; /* I1473g8b2: Cover Flow dim colour 0xAARRGGBB (theme token SCRIM, set by the UI); 0 = black */
} mui_grid_style;

/* I1473g2: the tile model, shared by Grid and Cover Flow. Fills `out` (at most `cap`) from the engine view window: one tile per
 * non-heading row, in window order, title/artist/format pointing into the view (valid until the next pump), art_id and kind as the
 * desktop row_kind. Returns the tiles written; NULL view or rows give 0. */
uint32_t mui_grid_tiles(const sel_view_state* v, mui_tile* out, uint32_t cap);
/* I1473g2: grid style for a screen `screen_w` pixels wide with the theme pens: cover edge 96 below 1024, 128 from 1024 (D5). */
mui_grid_style mui_grid_style_for(int32_t screen_w, uint32_t background, uint32_t tile, uint32_t selected);

/* Columns of a grid `width` pixels wide (at least 1). */
uint32_t mui_grid_cols(const mui_grid_style* s, int32_t width);
/* Rectangle of tile i (canvas coordinates) and of its cover. */
sel_gui_rect mui_grid_tile_rect(const mui_grid_style* s, int32_t width, uint32_t i);
/* I1473g4: tile rows that fit a canvas `height` tall (at least 1), and the first row drawn so the cursor row stays on screen
 * (sel_uim_grid_first_row over the rows that fit). */
uint32_t mui_grid_visible_rows(const mui_grid_style* s, int32_t height);
uint32_t mui_grid_first_row(const mui_grid_style* s, int32_t width, int32_t height, uint32_t cursor);
/* I1473g7: input for the tile canvases (Grid now, Cover Flow in rows g8b4/g8b5), pure so the host test drives it. `row_step` is the
 * tiles a vertical move or one wheel notch jumps: the column count for Grid; 0 for Cover Flow (left/right and the wheel step one cover,
 * up/down do nothing). Amiga RAWKEY codes (devices/rawkeycodes.h) and NewMouse wheel codes. */
typedef enum mui_grid_input_kind {
    MUI_GIN_NONE = 0,
    MUI_GIN_MOVE,     /* arg = signed tile delta (engine VIEW_CURSOR -n / +n) */
    MUI_GIN_PICK,     /* arg = tile index: the cursor goes there (click, right-click) */
    MUI_GIN_ACTIVATE, /* the cursor tile opens or plays (Enter) */
    MUI_GIN_OPEN      /* arg = tile index: pick, then activate (double click) */
} mui_grid_input_kind;
typedef struct mui_grid_input {
    uint32_t kind; /* mui_grid_input_kind */
    int32_t arg;
} mui_grid_input;
#define MUI_RAWKEY_RETURN 0x44u
#define MUI_RAWKEY_ENTER 0x43u
#define MUI_RAWKEY_UP 0x4Cu
#define MUI_RAWKEY_DOWN 0x4Du
#define MUI_RAWKEY_RIGHT 0x4Eu
#define MUI_RAWKEY_LEFT 0x4Fu
#define MUI_RAWKEY_WHEEL_UP 0x7Au
#define MUI_RAWKEY_WHEEL_DOWN 0x7Bu
/* A key-down (or wheel) rawkey code; 1 and *out filled when it is ours, else 0. */
int mui_grid_key(uint32_t rawkey, uint32_t row_step, mui_grid_input* out);
/* Tile under the pointer at canvas coordinates (x, y), -1 for a gap, the margin or no tile. `cursor` is the tile index the last
 * draw used (it fixes the first row drawn). */
int32_t mui_grid_hit(const mui_grid_style* s, int32_t width, int32_t height, uint32_t n, uint32_t cursor, int32_t x, int32_t y);
/* Tile index (non-heading rows, as mui_grid_tiles) to the engine window row, -1 when out of range. */
int32_t mui_grid_tile_row(const sel_view_state* v, uint32_t tile);

/* I1473g4: redraw signature of what the grid shows (titles, artists, art ids, kinds, count, cursor); equal = nothing to redraw. */
uint32_t mui_grid_signature(const mui_tile* tiles, uint32_t n, uint32_t cursor);
/* Draws the tiles into the canvas: background, then per tile from row `first_row` on a card (cover edge + `s->caption` of room under
 * it for the caption the class draws with Text()), the cover from `art` or the platform cover, and the selection outline on `cursor`
 * (an index into `tiles`). Returns the tiles drawn (those on screen). */
uint32_t mui_canvas_draw_grid(mui_canvas* m, const mui_grid_style* s, const mui_tile* tiles, uint32_t n, uint32_t cursor,
                              uint32_t first_row, mui_art_fn art, void* art_ctx);

/* I1473g8b2: Cover Flow on the RTG canvas, all geometry from the shared sel_uim_flow_* (include/selecta/uimodel/flow.h). The tiles are the
 * view window (window_start 0); `s->edge` is the edge of the square art words `art` returns (the shared 144 px thumbnail, D-F1: the front
 * cover is upscaled); `s->caption` is the caption block height under the front cover. `centre_q16` is the fractional centre (16.16). */
#include <selecta/uimodel/flow.h>
typedef struct mui_flow_view {
    uint32_t n;                                 /* covers laid out */
    sel_uim_flow_cover cv[SEL_UIM_FLOW_MAX];    /* draw order, far to near */
    int32_t big;                                /* centre cover edge */
    sel_gui_rect caption;                       /* the caption block under the front cover (canvas coordinates) */
} mui_flow_view;
void mui_flow_layout(const mui_grid_style* s, int32_t w, int32_t h, int64_t centre_q16, uint32_t n, mui_flow_view* out);
/* Draws background, covers (art or platform cover, rounded), scrim and the front cover's shadow. Returns the covers drawn. */
uint32_t mui_canvas_draw_flow(mui_canvas* m, const mui_grid_style* s, const mui_tile* tiles, uint32_t n, int64_t centre_q16, mui_art_fn art,
                              void* art_ctx, mui_flow_view* view);
/* D-F2: glide (220 ms ease-out, the Mac's) only on a 68040 or better; 68020/68030 step one frame per key. */
int mui_flow_glides(int cpu_is_040_or_better);
/* Glide progress 16.16 (0..65536) after `elapsed_ms`; 65536 = at rest. Same easing table as the Mac (SEL_GUI_EASE_OUT, SEL_GUI_ANIM_NORMAL_MS). */
int32_t mui_flow_glide_q16(uint32_t elapsed_ms);

/* I1473g5: AGA (planar) grid. The geometry and the cover indices are pure; the class supplies a sink that does RectFill and
 * WriteChunkyPixels. `rect` gets 0x00RRGGBB (background, card, selection outline: three pens); `cover` gets the cover square and its
 * edge x edge cube indices 0..MUI_COVER_CUBE-1 (idx NULL: no art yet, paint `tint` 0x00RRGGBB instead). Rects are in canvas
 * coordinates and may run past the canvas (the sink clips). The index cache holds each cover once (remapped on first use). */
typedef struct mui_aga_sink {
    void (*rect)(void* ctx, sel_gui_rect r, uint32_t rgb);
    void (*cover)(void* ctx, sel_gui_rect r, const uint8_t* idx, uint32_t tint);
    /* I1473g8b3: Cover Flow strip, r.w x r.h cube indices already scaled, clipped to the visible part and dimmed (NULL: flow not drawn). */
    void (*chunky)(void* ctx, sel_gui_rect r, const uint8_t* idx);
} mui_aga_sink;
typedef struct mui_aga_cache mui_aga_cache;
mui_aga_cache* mui_aga_cache_open(int32_t edge, uint32_t slots);
void mui_aga_cache_close(mui_aga_cache* c);
/* 0x00RRGGBB platform tint of a tile (the planar stand-in for the generated cover). */
uint32_t mui_tile_tint(const mui_tile* t);
/* Draws like mui_canvas_draw_grid into a w x h canvas through `sink`; returns the tiles drawn. */
uint32_t mui_canvas_draw_grid_aga(const mui_grid_style* s, int32_t w, int32_t h, const mui_tile* tiles, uint32_t n, uint32_t cursor,
                                  uint32_t first_row, mui_art_fn art, void* art_ctx, mui_aga_cache* cache, const mui_aga_sink* sink,
                                  void* sink_ctx);

/* I1473g8b3: Cover Flow on a palette screen (design 4). Layout from mui_flow_layout (view, may be NULL); each cover is clipped to the strip
 * not hidden by nearer covers, so every pixel is written once; scrim = cube index dimmed through mui_cover_dim_lut (alpha snaps to 0/90/150).
 * No art: `sink->cover` with a dimmed tint. Returns the covers drawn. `cache` edge must equal s->edge. No rounded corners, no shadow. */
uint32_t mui_canvas_draw_flow_aga(const mui_grid_style* s, int32_t w, int32_t h, const mui_tile* tiles, uint32_t n, int64_t centre_q16,
                                  mui_art_fn art, void* art_ctx, mui_aga_cache* cache, const mui_aga_sink* sink, void* sink_ctx,
                                  mui_flow_view* view);
/* Cube index -> cube index of its colour darkened by `alpha` (0..255) towards `scrim_rgb` (0x00RRGGBB). */
void mui_cover_dim_lut(int32_t alpha, uint32_t scrim_rgb, uint8_t lut[27]);

/* Artwork path (MUI-P5-4): RTG screens decode the cover through picture.datatype and scale it with
 * sel_gui_image_scale; planar screens show the platform cover. */
typedef enum mui_art_path { MUI_ART_COVER = 0, MUI_ART_DATATYPE = 1 } mui_art_path;
mui_art_path mui_art_path_for(int screen_is_rtg);
/* Draws one cover into `r` by path: DATATYPE scales the decoded words `src` (sw x sh) to r and blits them, COVER (or a
 * NULL src) draws the platform cover. Returns the path actually used. */
/* The baked icon set the cover kind icons come from (I1399a); the UI plugin passes its table, NULL draws tint only. */
void mui_canvas_set_icons(const sel_gui_icons_packed* baked);

mui_art_path mui_canvas_draw_art(mui_canvas* m, sel_gui_rect r, mui_art_path want, const uint32_t* src, int32_t sw,
                                 int32_t sh, uint32_t kind, const char* format, const char* title);

/* I1440q3: centre-crops the w x h words `src` to a square and scales it into `dst` (edge x edge). The real cover (library blob decoded
 * by the plugin) becomes the canvas image; the frame is square, so no stretch. 0 on bad arguments. */
int mui_image_square(const uint32_t* src, int32_t w, int32_t h, uint32_t* dst, int32_t edge);

/* I1440q4: AGA (planar, palette) cover. The words (edge x edge) become indices 0..MUI_COVER_CUBE-1 of a 3 x 3 x 3 colour cube with a
 * 4 x 4 ordered dither (integer only, one pass, no error buffer: roughly 60 cycles a pixel, ~1M cycles = ~70 ms on a 14 MHz 68020 for
 * 128 x 128, estimate). mui_cover_cube_rgb gives 0x00RRGGBB of an index (the pen to obtain); mui_cover_scale8 nearest-scales the
 * square index image to dw x dh. */
#define MUI_COVER_CUBE 27u
uint32_t mui_cover_cube_rgb(uint32_t index);
int mui_cover_remap(const uint32_t* src, int32_t edge, uint8_t* idx);
void mui_cover_scale8(const uint8_t* src, int32_t sedge, uint8_t* dst, int32_t dw, int32_t dh);

#endif /* MUI_CANVAS_H */
