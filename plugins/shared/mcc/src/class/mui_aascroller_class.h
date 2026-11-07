/* Selecta_AAScroller (I667i, design 2026-10-28-mui-custom-classes.md section 3.5). MUI 3.8 Area subclass: the flat thumb-only scroll bar of
 * the Selecta lists (track SURFACE, thumb TRACK, SURFACE_PRESSED while hovered or dragged, no arrow buttons, no light Prop box). The owner
 * (AAList) sets UPA_Scroller_Visible and UPA_Scroller_Entries; the user drags the thumb or clicks the track, which sets UPA_Scroller_First (notifies). Amiga only; the
 * thumb geometry and the colours are in map/mui_aa.c (mui_aa_scroller_*). */
#ifndef MUI_AASCROLLER_CLASS_H
#define MUI_AASCROLLER_CLASS_H

#include <mcc/up.h>
#include <stdint.h>


/* Scrollers constructed since start (sentinel: AAList builds exactly one per list). */
uint32_t mui_aascroller_made(void);
void mui_aascroller_class_free(void);

#endif
