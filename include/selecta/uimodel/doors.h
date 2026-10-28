/* The sidebar doors (MUI-P4-0, DUI-P4-1): one table the desktop UI and the MUI UI both build their sidebar from. A door
 * is a label and the page it opens; the page is named as the desktop --view names it ("home", "search", ...), so the
 * table carries no UI-specific enum. Icons are presentation and stay with each UI. Pure C99. */
#ifndef SELECTA_UIMODEL_DOORS_H
#define SELECTA_UIMODEL_DOORS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEL_UIM_DOOR_COUNT 7u

typedef struct sel_uim_door {
    const char* label; /* the full English row label */
    const char* page;  /* the page name: home, search, library, playlists, queue, nowplaying, songinfo */
    const char* shorter; /* I537: the label drawn when the full one does not fit the rail (NULL: none) */
    uint32_t topbar;     /* I615a: 1 when the window-wide top bar (layout topbar) owns this page: the sidebar then omits the row */
} sel_uim_door;

/* Door i in sidebar order, NULL past SEL_UIM_DOOR_COUNT. */
const sel_uim_door* sel_uim_door_at(uint32_t i);

/* I615a: whether door i has a sidebar row when the layout has a top bar (top_bar != 0) or none. Without a top bar every door
 * shows (the bitmap tier, the mini layout, the MUI sidebar). */
int sel_uim_door_shown(uint32_t i, int top_bar);

/* I615a: the row slot of door i in the sidebar: the shown doors before it. A hidden door has no slot (returns 0). */
uint32_t sel_uim_door_slot(uint32_t i, int top_bar);

#ifdef __cplusplus
}
#endif
#endif
