/* MUI plugin mapping functions (plan 2026-10-25-amiga-mui.md section 2.2). Pure C: no <proto/muimaster.h>, so the
 * host builds and tests this directory (MUI-P1-1). */
#ifndef SEL_MUI_MAP_H
#define SEL_MUI_MAP_H

#include <stdint.h>

/* The view styles the MUI UI offers. An RTG screen can host the custom canvas (grid, cover flow), so every style;
 * a planar screen has only native list widgets, so the classic list styles (plan section 1, finish line 3). */
uint32_t mui_style_mask(int screen_is_rtg);

#endif
