/* Selecta_AAButton (I665 spike, I662 route): MUI 3.8 Area subclass that draws a rounded rectangle in ONE flat colour
 * (theme token SURFACE_RAISED, pressed colour derived by mui_aa_pressed_rgb) with an antialiased caption (aa_draw).
 * Amiga only; the geometry and ramp picks are in map/mui_aa.c and are tested on the host. */
#ifndef MUI_AABTN_CLASS_H
#define MUI_AABTN_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

/* face and text are 0xRRGGBB theme tokens; widest is the longest caption the button will show (sets the minimum width). */
Object* mui_aabtn_new(const char* caption, const char* widest, uint32_t face, uint32_t text, LONG id);
/* New caption (play/pause); redraws. */
void mui_aabtn_set_caption(Object* obj, const char* caption);
/* Which backend drew the last frame: 0 none yet, 1 plain MUI text (fallback), 2 ttengine truecolour, 3 ttengine palette ramp. */
int mui_aabtn_backend(Object* obj);
void mui_aabtn_class_free(void);

#endif
