/* MUI-P2-1 / MUI-P2-2: layout regions to group weights and the visible set per breakpoint. Pure C over the uimodel
 * layout tree (sel_uim_layout_compute, sel_uim_breakpoint_for), so the host tests what the Amiga build applies. */
#include <selecta/uimodel.h>

#include <string.h>

#include "mui_icons.h"
#include "mui_map.h"

static const char* const k_slot_names[MUI_SLOT_COUNT] = {"suggestion", "sidebar", "main", "detail", "player", "topbar-slot"};

const char* const mui_tool_labels[MUI_TOOL_BTNS] = {"Like", "Remove From Queue", "Clear Queue", "Move Up", "Move Down", "Help", "Settings",
                                                    "New Playlist", "Open Playlist", "Rename Playlist", "Delete Playlist", "Clear Library"};

uint32_t mui_tool_row_chars(uint32_t row) {
    uint32_t i, n = 0;
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        if ((i < MUI_TOOL_ROW2 ? 0u : 1u) == row) {
            n += (uint32_t)strlen(mui_tool_labels[i]);
        }
    }
    return n;
}

uint32_t mui_tool_row_width(uint32_t row) {
    uint32_t i, n = 0;
    for (i = 0; i < MUI_TOOL_BTNS; i++) {
        if ((i < MUI_TOOL_ROW2 ? 0u : 1u) == row) {
            n += mui_button_width(mui_tool_labels[i], MUI_TOOL_ICON_ONLY);
        }
    }
    return n;
}

uint32_t mui_extra_row_width(void) {
    uint32_t i, n = 0;
    for (i = 0; i < MUI_EXTRA_COUNT; i++) {
        n += mui_button_width(mui_extra_label(i), MUI_EXTRA_ICON_ONLY);
    }
    return n;
}

uint32_t mui_transport_row_width(uint32_t row) {
    uint32_t b, n = row == 0u ? MUI_SLIDER_MIN : 0u;
    for (b = 0; b < MUI_BTN_COUNT; b++) {
        if ((b < MUI_TRANSPORT_ROW1 ? 0u : 1u) != row) {
            continue;
        }
        uint32_t st, widest = 0;
        for (st = 0; st < 12u; st++) { /* play (2) x repeat (3) x random (2) */
            sel_uim_frame f;
            const char* l;
            memset(&f, 0, sizeof f);
            f.play = (st & 1u) != 0u ? SEL_PLAYING : SEL_STOPPED;
            f.repeat_mode = (st >> 1) % 3u;
            f.random_on = (st / 6u) != 0u;
            l = mui_transport_label(b, &f);
            if (l != NULL && mui_button_width(l, mui_transport_icon_only(b)) > widest) {
                widest = mui_button_width(l, mui_transport_icon_only(b));
            }
        }
        n += widest;
    }
    return n;
}

void mui_layout_map(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier) {
    sel_uim_layout l;
    uint32_t i, s;
    memset(m, 0, sizeof *m);
    /* I615d (design 8.1): the tier follows the screen as in the rich UI. A native planar screen is the bitmap tier (I674: a 640
     * wide window keeps a rail, no mini-layout suggestion, no top bar); an RTG screen is the default tier with the top bar. */
    sel_uim_layout_compute_tier(&l, width, height, tier);
    m->width = width;
    m->height = height;
    m->breakpoint = l.breakpoint;
    m->tier = tier;
    /* I615d (design 8.2): the top bar is its own field of the layout, not a region. */
    m->show[MUI_SLOT_TOPBAR] = l.topbar.h > 0u;
    m->weight_w[MUI_SLOT_TOPBAR] = width;
    m->weight_h[MUI_SLOT_TOPBAR] = l.topbar.h;
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

uint32_t mui_layout_relayout(mui_layout_map_t* m, uint32_t width, uint32_t height, uint32_t tier, uint32_t* flips) {
    mui_layout_map_t next;
    uint32_t s, n = 0;
    mui_layout_map(&next, width, height, tier);
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
