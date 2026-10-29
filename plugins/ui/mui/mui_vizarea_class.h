/* Selecta_VizArea, the MUI custom class of the Now Playing visualizer (plan 2026-10-25-amiga-mui.md, MUI-P5-3, MUI-P5-5).
 * Amiga only; the plugin set, the frame gate and the render step are in map/mui_viz.c and are tested on the host. */
#ifndef MUI_VIZAREA_CLASS_H
#define MUI_VIZAREA_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

#include "map/mui_viz.h"

/* Creates the class (once) and one viz area drawing the plugin `set` selects; NULL when MUI refuses the class. `tokens` are
 * the 0xRRGGBB theme tokens (SEL_GUI_TOKEN_*) the palette is built from, the same mapping the desktop strip uses. The set
 * must outlive the object. */
Object* mui_vizarea_new(mui_vizset* set, const uint32_t* tokens, int screen_is_rtg);
/* Runs one step on the newest engine frame (valid until the next pump): when the gate lets it, renders and redraws.
 * Returns 1 when a frame was drawn. A planar screen draws nothing (no Picasso96) and counts nothing. */
int mui_vizarea_step(Object* obj, const sel_viz_frame* f, const sel_uim_budget* b, uint32_t now_ms);
/* Frames the plugin rendered into this object (the rig sentinel). */
uint32_t mui_vizarea_frames(Object* obj);
/* Deletes the class; call after the application object is disposed. */
void mui_vizarea_class_free(void);

#endif
