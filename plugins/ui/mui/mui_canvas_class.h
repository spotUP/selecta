/* Selecta_Canvas, the MUI custom class that hosts src/gui/draw output (plan 2026-10-25-amiga-mui.md, MUI-P5-1, MUI-P5-2).
 * Amiga only; the geometry and drawing it calls are in map/mui_canvas.c and are tested on the host. */
#ifndef MUI_CANVAS_CLASS_H
#define MUI_CANVAS_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

#include "map/mui_canvas.h"

/* Creates the class (once) and one canvas object; NULL when MUI refuses the class. The object is created hidden
 * (MUIA_ShowMe FALSE): the style switch of MUI-P3-5 shows it. */
Object* mui_canvas_new(const mui_grid_style* style, int screen_is_rtg);
/* Tiles the next MUIM_Draw shows (copied by reference: the caller keeps them alive), the cursor tile, and the cover
 * lookup (the art cache); then redraws the object. */
void mui_canvas_set_grid(Object* obj, const mui_tile* tiles, uint32_t n, uint32_t cursor, mui_art_fn art, void* art_ctx);
/* I1399d: a canvas that draws one generated cover over the whole object (Now Playing art), shown at once; set_cover gives the
 * song's cover kind (SEL_COVER_KIND_*), file extension and title, then redraws. RTG draws the generated cover, planar (AGA) fills the kind tint (0x00RRGGBB) and draws the same label and title. */
Object* mui_canvas_cover_new(int screen_is_rtg);
void mui_canvas_set_cover(Object* obj, uint32_t kind, uint32_t tint, const char* ext, const char* title);
/* I1440q3: the real cover words (edge x edge) the single canvas draws instead of the generated cover; the canvas copies it, NULL
 * returns to the generated cover. */
void mui_canvas_set_image(Object* obj, const uint32_t* px, int32_t edge);
/* I1455/I1459: the download spinner over the cover: eight dots on a ring, `lead` the dot in `accent` (0..7), the rest in `muted`
 * (0x00RRGGBB). lead -1 takes it away. Redraws only when the lead or a colour changes. */
void mui_canvas_set_spin(Object* obj, int lead, uint32_t accent, uint32_t muted);
/* Grid frames drawn by this object (the rig sentinel). */
/* I1473g8b2: Cover Flow mode on the RTG canvas (tiles and cursor still come from mui_canvas_set_grid; the tiles are the view window).
 * mui_canvas_flow_tick: the app calls it while the last call returned 1 (about every 40 ms): redraws the next glide frame, 0 = at rest. */
void mui_canvas_set_flow(Object* obj, int on);
int mui_canvas_flow_tick(Object* obj);

/* I1473g4: forget the redraw signature so the next mui_canvas_set_grid redraws (a cover loaded, same tiles). */
void mui_canvas_invalidate(Object* obj);
uint32_t mui_canvas_draws(Object* obj);
/* I1473g6: tile columns and whole visible tile rows of the canvas as laid out; 0 when it has no size yet (hidden). */
void mui_canvas_grid_fit(Object* obj, uint32_t* cols, uint32_t* rows);
/* I1473g7: input for the shown canvas. mui_canvas_input_set turns it on (before the object is shown): arrows, Enter, the wheel and tile
 * clicks are queued and the application gets MUIM_Application_ReturnID `return_id`; mui_canvas_input_take pops one (1 = filled). The
 * tile index is into the tiles last given to mui_canvas_set_grid. */
void mui_canvas_input_set(Object* obj, ULONG return_id);
int mui_canvas_input_take(Object* obj, mui_grid_input* out);
/* Writes w x h 0xAARRGGBB words to the rastport at (dx, dy) through Picasso96 (the viz area shares it). */
void mui_canvas_push(struct Library* p96, const uint32_t* px, int32_t w, int32_t h, struct RastPort* rp, int32_t dx, int32_t dy);
/* Deletes the class; call after the application object is disposed. */
void mui_canvas_class_free(void);

#endif
