/* Selecta_AAScroller (I667i, design 2026-10-28-mui-custom-classes.md section 3.5). MUI 3.8 Area subclass: the flat thumb-only scroll bar of
 * the Selecta lists (track SURFACE, thumb TRACK, SURFACE_PRESSED while hovered or dragged, no arrow buttons, no light Prop box). The owner
 * (AAList) sets AAS_Visible and AAS_Entries; the user drags the thumb or clicks the track, which sets AAS_First (notifies). Amiga only; the
 * thumb geometry and the colours are in map/mui_aa.c (mui_aa_scroller_*). */
#ifndef MUI_AASCROLLER_CLASS_H
#define MUI_AASCROLLER_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>
#include <mcc/up.h>

#define AAS_First UP_TAG(UP_CLS_SCROLLER, 0x01)   /* LONG: first visible entry (notifies) */
#define AAS_Visible UP_TAG(UP_CLS_SCROLLER, 0x02) /* LONG: entries that fit the owner */
#define AAS_Entries UP_TAG(UP_CLS_SCROLLER, 0x03) /* LONG: entries in the owner */

Object* mui_aascroller_new(void);
/* Scrollers constructed since start (sentinel: AAList builds exactly one per list). */
uint32_t mui_aascroller_made(void);
void mui_aascroller_class_free(void);

#endif
