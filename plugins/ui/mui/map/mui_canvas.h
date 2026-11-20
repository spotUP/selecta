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
/* Draws the tiles into the canvas: background, then per tile a card, the cover from `art` or the platform cover, and the
 * selection outline on `cursor`. Returns the tiles drawn (those that fit the canvas height). */
uint32_t mui_canvas_draw_grid(mui_canvas* m, const mui_grid_style* s, const mui_tile* tiles, uint32_t n, uint32_t cursor,
                              mui_art_fn art, void* art_ctx);

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
