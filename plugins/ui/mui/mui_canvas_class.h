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
/* Grid frames drawn by this object (the rig sentinel). */
uint32_t mui_canvas_draws(Object* obj);
/* Deletes the class; call after the application object is disposed. */
void mui_canvas_class_free(void);

#endif
