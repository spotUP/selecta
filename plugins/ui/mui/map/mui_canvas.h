/* Canvas of the MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P5-1, MUI-P5-2, MUI-P5-4): the pure part of the Selecta_Canvas
 * class. The class (mui_canvas_class.c, Amiga only) owns one of these per object; everything it decides lives here so
 * host tests reach it: canvas geometry, the tile grid drawn with src/gui/draw, and the artwork path per screen kind. */
#ifndef MUI_CANVAS_H
#define MUI_CANVAS_H

#include <selecta/gui/gui_draw.h>

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
    const char* format; /* file extension; its platform family colours the cover (NULL = Music) */
    int64_t art_id;
} mui_tile;

/* Cover lookup: tile-edge square words of the cover with `art_id`, NULL on a miss (the tile then shows the platform cover).
 * Backed by sel_gui_art_cache_get in the class (MUI-P5-2). */
typedef const uint32_t* (*mui_art_fn)(void* ctx, int64_t art_id);

typedef struct mui_grid_style {
    uint32_t background, tile, selected; /* 0xAARRGGBB, filled from MUI pens by the class */
    int32_t edge, gap, pad;              /* cover edge, gap between tiles, padding inside a tile */
} mui_grid_style;

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
mui_art_path mui_canvas_draw_art(mui_canvas* m, sel_gui_rect r, mui_art_path want, const uint32_t* src, int32_t sw,
                                 int32_t sh, const char* format, const char* title);

#endif /* MUI_CANVAS_H */
