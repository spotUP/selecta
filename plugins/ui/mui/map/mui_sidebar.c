/* I793 (drift doc S3 to S9, design 2026-10-28-mui-custom-classes.md): the MUI Your Library sidebar model. Pure C99 with no MUI
 * calls, so the host contract test reaches it. It follows the Mac side_lib_draw (desktop_ui.c): the doors (Browse Folders hidden
 * because the header opens that page), the header with Create Playlist and Expand, the kind chips, the item rows, the empty
 * state and the Create your first playlist card. Every text comes from uimodel/browse.h, every door from uimodel/doors.h. */
#include "mui_map.h"
#include "mui_icons.h"
#include <selecta/library_item.h>
#include <selecta/uimodel/browse.h>
#include <selecta/uimodel/doors.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_builds, g_rail_builds;

uint32_t mui_sidebar_builds(void) {
    return g_builds;
}

uint32_t mui_sidebar_rail_builds(void) {
    return g_rail_builds;
}

static void put(char* dst, size_t cap, const char* src) {
    snprintf(dst, cap, "%s", src != NULL ? src : "");
}

int mui_sidebar_chips_single(int bitmap, uint32_t sidebar_px, int aspect_q8) {
    int32_t w[SEL_UIM_SIDE_CHIPS];
    uint32_t k;
    for (k = 0; k < SEL_UIM_SIDE_CHIPS; k++) {
        w[k] = (int32_t)mui_button_width(sel_uim_side_chip_name(k), 0, aspect_q8);
    }
    return sel_uim_lfilter_chips_single(bitmap, sel_uim_lfilter_chips_total(w, SEL_UIM_SIDE_CHIPS, 4), (int32_t)sidebar_px - 2 * MUI_BTN_PAD_X);
}

void mui_sidebar_build(const mui_sidebar_in* in, mui_sidebar_model* m) {
    uint32_t i, k, mask, playlists = 0;
    memset(m, 0, sizeof *m);
    g_builds++;
    m->rail = in->rail != 0;
    if (m->rail) {
        /* I949c: the labelled door rail (uimodel SIDEBAR_RAIL_LABELLED): every door keeps its row, the Library door included (the
         * icon-only/rail form has no "Your Library" header, sel_uim_door_shown_in labelled 0), and nothing else is built. The Library door
         * opens the Library page, which carries the chips, rows, empty state and card (I673). */
        g_rail_builds++;
        for (i = 0; i < SEL_UIM_DOOR_COUNT && m->door_n < MUI_SIDEBAR_DOORS; i++) {
            if (sel_uim_door_shown_in(i, in->top_bar, 0)) {
                m->door[m->door_n++] = i;
            }
        }
        return;
    }
    put(m->brand, sizeof m->brand, "Selecta"); /* S1: the Mac draw_sidebar brand (accent tile with the play icon, then the name) */
    put(m->brand_icon, sizeof m->brand_icon, in->bitmap ? "" : "play");
    for (i = 0; i < SEL_UIM_DOOR_COUNT && m->door_n < MUI_SIDEBAR_DOORS; i++) { /* S3: the library header is always there on MUI */
        if (sel_uim_door_shown_in(i, in->top_bar, 1)) {
            m->door[m->door_n++] = i;
        }
    }
    put(m->title, sizeof m->title, sel_uim_side_text_get(SEL_UIM_SIDE_TITLE)); /* S4 */
    put(m->create, sizeof m->create, sel_uim_side_text_get(SEL_UIM_SIDE_CREATE));
    put(m->expand, sizeof m->expand, sel_uim_side_text_get(in->wide ? SEL_UIM_SIDE_COLLAPSE : SEL_UIM_SIDE_EXPAND));
    m->chip_single = in->chips_single != 0;
    m->chip_left_clear = sel_uim_lfilter_left_clears(in->chip);
    put(m->chip_left_name, sizeof m->chip_left_name, sel_uim_lfilter_left_name(in->chip));
    put(m->chip_label, sizeof m->chip_label, sel_uim_lfilter_label(in->chip));
    for (k = 0; k < SEL_UIM_SIDE_CHIPS; k++) { /* S5: a selected chip hides the others; the single label hides them all */
        m->chip_shown[k] = !m->chip_single && (in->chip == 0u || in->chip == k + 1u);
        put(m->chip[k], sizeof m->chip[k], sel_uim_side_chip_name(k));
    }
    m->chip_selected = in->chip;
    m->chip_clear = in->chip != 0u;
    m->by_you = in->chip == 1u;
    mask = sel_uim_lfilter_mask(in->chip);
    for (i = 0; i < in->item_n; i++) { /* S6: the same filter and kind mask as side_lib_reload */
        const sel_engine_library_item* it = &in->items[i];
        mui_sidebar_row* r;
        if ((mask & SEL_LIB_ITEM_BIT(it->kind)) == 0u || !sel_uim_lfilter_match(it->name, in->filter != NULL ? in->filter : "")) {
            continue;
        }
        if (in->chip == 1u && in->by_you_only && strcmp(it->creator, "You") != 0) {
            continue;
        }
        playlists += it->kind == (uint32_t)SEL_LIB_ITEM_PLAYLIST;
        if (m->row_n < MUI_SIDEBAR_ROWS) {
            r = &m->row[m->row_n++];
            put(r->mark, sizeof r->mark, sel_uim_library_item_mark(it->kind, it->pinned));
            put(r->title, sizeof r->title, it->name);
            sel_uim_library_item_subtitle(it->kind, it->sub, it->songs, r->subtitle, sizeof r->subtitle);
            put(r->type, sizeof r->type, sel_uim_library_item_type(it->kind)); /* S7: the Expand columns */
            r->kind = it->kind;
            r->id = it->id;
        }
    }
    m->wide = in->wide != 0u; /* S7: Title, Type, Date Added, Played */
    m->cols = in->wide ? 4u : 1u;
    put(m->empty, sizeof m->empty, sel_uim_side_empty(in->chip, m->row_n, in->bitmap)); /* S8 */
    m->card = sel_uim_side_card_shown(in->chip, in->filter != NULL && in->filter[0] != '\0', playlists, in->bitmap); /* S9 */
    if (m->card) {
        put(m->card_title, sizeof m->card_title, sel_uim_side_text_get(SEL_UIM_SIDE_CARD_TITLE));
        put(m->card_body, sizeof m->card_body, sel_uim_side_text_get(SEL_UIM_SIDE_CARD_BODY));
        put(m->card_button, sizeof m->card_button, sel_uim_side_text_get(SEL_UIM_SIDE_CARD_BUTTON));
    }
}
