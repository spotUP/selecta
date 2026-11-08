/* I798 (design 2026-10-30-mui-mac-drift.md rows O1, O3, T2, R3): the command palette window, the tooltip texts and the Hide / Show
 * Now Playing toggle. Pure C over the shared uimodel (txt_palette, sel_uim_tip_*, sel_uim_action_tip_text), so the host tests what
 * the Amiga build shows. The palette table is the Mac's: no command list is kept here. */
#include <selecta/protocol.h>
#include <selecta/uimodel.h>
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/keybind.h>
#include <selecta/uimodel/palette.h>

#include <stdio.h>
#include <string.h>

#include "mui_map.h"

static uint32_t g_tip_calls, g_np_toggles;

void mui_palette_filter(mui_palette* p, const char* query) {
    uint32_t i;
    txt_palette_set_query(&p->pal, query != NULL ? query : "");
    snprintf(p->query, sizeof p->query, "%s", p->pal.query);
    for (i = 0; i < p->pal.count; i++) {
        const txt_palette_item* it = &p->pal.items[i];
        snprintf(p->row[i], sizeof p->row[i], "%-22s %-24s %s", it->name, txt_palette_area_title(it->area), it->key);
    }
    p->count = p->pal.count;
}

void mui_palette_open(mui_palette* p) {
    txt_bindings_default(&p->binds);
    txt_palette_open(&p->pal, &p->binds);
    p->opens++;
    mui_palette_filter(p, "");
}

const char* mui_palette_row(void* ctx, long row) {
    const mui_palette* p = (const mui_palette*)ctx;
    return row >= 0 && (uint32_t)row < p->count ? p->row[row] : "";
}

uint32_t mui_palette_pick(mui_palette* p, long row, mui_send_fn send, void* ctx, char* fill, uint32_t cap) {
    const char* verb;
    if (row < 0 || (uint32_t)row >= p->count) {
        return MUI_PAL_NONE;
    }
    p->pal.cursor = (uint32_t)row;
    verb = txt_palette_picked(&p->pal);
    if (verb == NULL) {
        return MUI_PAL_NONE;
    }
    snprintf(p->last_verb, sizeof p->last_verb, "%s", verb);
    if (txt_palette_verb_needs_args(verb)) {
        snprintf(fill, cap, "%s ", verb);
        p->fills++;
        return MUI_PAL_FILL;
    }
    p->runs++;
    return send != NULL && send(ctx, verb) != 0 ? MUI_PAL_RUN : MUI_PAL_REJECTED;
}

uint32_t mui_tip(uint32_t kind, uint32_t index, const sel_uim_frame* f, int flag, char* out, uint32_t cap) {
    const char* text = NULL;
    g_tip_calls++;
    out[0] = '\0';
    switch (kind) {
    case MUI_TIP_TRANSPORT:
        return mui_tooltip_transport(index, f, out, cap);
    case MUI_TIP_DOOR:
        return mui_tooltip_door(index, out, cap);
    case MUI_TIP_LIKE:
        text = sel_uim_tip_like(flag);
        break;
    case MUI_TIP_NP_PANEL:
        text = sel_uim_tip_np_panel(flag);
        break;
    case MUI_TIP_SEARCH:
        text = "Search the library";
        break;
    case MUI_TIP_CMD:
        text = SEL_UIM_CMD_HINT;
        break;
    case MUI_TIP_PALETTE:
        text = "Command Palette";
        break;
    case MUI_TIP_SEEK:
        text = "Seek";
        break;
    case MUI_TIP_VOLUME:
        text = "Volume";
        break;
    default:
        break;
    }
    return text != NULL ? (uint32_t)snprintf(out, cap, "%s", text) : 0u;
}

uint32_t mui_tip_calls(void) { return g_tip_calls; }

uint32_t mui_np_toggle(mui_layout_map_t* m) {
    mui_layout_map_wide(m, m->width, m->height, m->tier, m->detail_hidden ? 0u : 1u, m->side_wide);
    g_np_toggles++;
    return m->detail_hidden;
}

uint32_t mui_np_toggles(void) { return g_np_toggles; }

static uint32_t g_side_toggles;

uint32_t mui_side_wide_toggle(mui_layout_map_t* m) {
    mui_layout_map_wide(m, m->width, m->height, m->tier, m->detail_hidden, m->side_wide ? 0u : 1u);
    g_side_toggles++;
    return m->side_wide;
}

uint32_t mui_side_toggles(void) { return g_side_toggles; }
