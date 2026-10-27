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
} sel_uim_door;

/* Door i in sidebar order, NULL past SEL_UIM_DOOR_COUNT. */
const sel_uim_door* sel_uim_door_at(uint32_t i);

#ifdef __cplusplus
}
#endif
#endif
