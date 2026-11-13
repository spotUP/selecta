/* Selecta_AAScrollGroup (I1351): a vertical scrolling page with our AAScroller instead of the stock MUI Scrollgroup bar. MUI 3.8 only.
 * Amiga only. */
#ifndef MUI_SCROLLGROUP_CLASS_H
#define MUI_SCROLLGROUP_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>

/* Horizontal group: a Virtgroup around `contents` and an AAScroller. Like a Scrollgroup it has no minimum height of its own. The
 * AAScroller (units: pixels) is kept in step with MUIA_Virtgroup_Top / MUIA_Virtgroup_Height; the mouse wheel scrolls it.
 * hidden: starts with MUIA_ShowMe FALSE. Takes `contents`; NULL when MUI refuses the class or an object (all disposed). */
Object* mui_aa_scrollgroup_new(Object* contents, int hidden);
/* Deletes the class; call after the application object is disposed. */
void mui_scrollgroup_class_free(void);

#endif
