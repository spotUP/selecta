/* Selecta_AABalance (I667l, design 2026-10-28-mui-custom-classes.md section 3.10): MUI 3.8 Balance subclass, the pane divider as a flat
 * line in the panel gap (BACKGROUND box, 1 px DIVIDER line). Dragging stays MUI's. Amiga only; width and line maths are in map/mui_aa.c
 * and tested on the host (mui-balance-geom). */
#ifndef MUI_AABALANCE_CLASS_H
#define MUI_AABALANCE_CLASS_H

#include <exec/types.h>
#include <libraries/mui.h>
#include <stdint.h>

Object* mui_aabalance_new(void);
/* Dividers constructed since start (sentinel: equals the number of balance sites the window build made, 2). */
uint32_t mui_aabalance_made(void);
void mui_aabalance_class_free(void);

#endif
