/* MUI-P2-1 / MUI-P2-2: layout regions to group weights and the visible set per breakpoint. Pure C over the uimodel
 * layout tree (sel_uim_layout_compute, sel_uim_breakpoint_for), so the host tests what the Amiga build applies. */
#include <selecta/uimodel.h>

#include <string.h>

#include "mui_map.h"

static const char* const k_slot_names[MUI_SLOT_COUNT] = {"suggestion", "sidebar", "main", "detail", "player"};

const char* const mui_tool_labels[MUI_TOOL_BTNS] = {"Like", "Remove From Queue", "Clear Queue", "Move Up", "Move Down", "Help", "Settings",
                                                    "Open Playlist", "Rename Playlist", "Delete Playlist", "Clear Library"};

uint32_t mui_tool_row_chars(uint32_t row) {
    uint32_t i, n = 0;
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        if ((i < MUI_TOOL_ROW2 ? 0u : 1u) == row) {
            n += (uint32_t)strlen(mui_tool_labels[i]);
        }
    }
    return n;
}

void mui_layout_map(mui_layout_map_t* m, uint32_t width, uint32_t height) {
    sel_uim_layout l;
    uint32_t i, s;
    memset(m, 0, sizeof *m);
    sel_uim_layout_compute(&l, width, height);
    m->width = width;
    m->height = height;
    m->breakpoint = l.breakpoint;
    for (i = 0; i < l.count; i++) {
        for (s = 0; s < MUI_SLOT_COUNT; s++) {
            if (strcmp(l.regions[i].name, k_slot_names[s]) == 0) {
                m->show[s] = 1;
                m->weight_w[s] = l.regions[i].w;
                m->weight_h[s] = l.regions[i].h;
            }
        }
    }
    /* Balance objects sit between two shown panels of the body row. */
    m->balance_sidebar_main = m->show[MUI_SLOT_SIDEBAR] && m->show[MUI_SLOT_MAIN];
    m->balance_main_detail = m->show[MUI_SLOT_MAIN] && m->show[MUI_SLOT_DETAIL];
}

uint32_t mui_layout_relayout(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t* flips) {
    mui_layout_map_t next;
    uint32_t s, n = 0;
    mui_layout_map(&next, width, height);
    if (next.breakpoint == m->breakpoint) {
        /* Same breakpoint: only the weights follow the new size, no object appears or disappears. */
        *m = next;
        if (flips != NULL) {
            *flips = 0;
        }
        return 0;
    }
    for (s = 0; s < MUI_SLOT_COUNT; s++) {
        n += m->show[s] != next.show[s];
    }
    *m = next;
    if (flips != NULL) {
        *flips = n;
    }
    return 1;
}
