/* MUI parity rows of plan 2026-10-25-amiga-mui.md section P8 (MUI-P8-1..15). Pure C, host-testable. Every action is read
 * from the shared action table (src/uimodel/actions.c) the desktop UI reads; the models are sel_uim_*. */
#include <selecta/uimodel.h>
#include <selecta/uimodel/doors.h>

#include <stdio.h>
#include <string.h>

#include "mui_map.h"

static uint32_t g_fires;

uint32_t mui_action_fires(void) { return g_fires; }

uint32_t mui_action_fire(const char* id, mui_send_fn send, void* ctx) {
    const char* line = mui_menu_line(id);
    g_fires++;
    return line != NULL && send != NULL && send(ctx, line) != 0;
}

static const sel_uim_action* action_by_id(const char* id) {
    uint32_t i, n = sel_uim_action_count();
    for (i = 0; id != NULL && i < n; i++) {
        if (strcmp(sel_uim_action_at(i)->id, id) == 0) {
            return sel_uim_action_at(i);
        }
    }
    return NULL;
}

static const char* const k_extras[MUI_EXTRA_COUNT] = {"next_album", "prev_album", "pitch_up", "pitch_down", "tempo_up", "tempo_down"};

const char* mui_extra_action(uint32_t i) { return i < MUI_EXTRA_COUNT ? k_extras[i] : NULL; }

const char* mui_extra_label(uint32_t i) {
    const sel_uim_action* a = action_by_id(mui_extra_action(i));
    return a != NULL ? a->label : NULL;
}

const char* mui_extra_tip(uint32_t i) {
    const sel_uim_action* a = action_by_id(mui_extra_action(i));
    return a != NULL ? a->tooltip : NULL;
}

#include <selecta/uimodel/fkeys.h>
#include <selecta/uimodel/keys.h>

uint32_t mui_fkeys_build(mui_fkey_bind* out, uint32_t cap) {
    uint32_t n = 0, store, slot;
    for (store = 0; store < 2u; store++) {
        for (slot = 1; slot <= SEL_UIM_FKEY_COUNT; slot++) {
            if (n < cap) {
                out[n].code = TXT_KEY_F1 + slot - 1u;
                out[n].mods = store != 0u ? TXT_MOD_SHIFT : 0u;
                out[n].slot = slot;
                out[n].store = (int)store;
            }
            n++;
        }
    }
    return n;
}

uint32_t mui_fkey_fire(uint32_t code, uint32_t mods, mui_send_fn send, void* ctx) {
    char lines[SEL_UIM_FKEY_LINES][SEL_UIM_FKEY_LINE_MAX];
    int store = 0;
    uint32_t slot = sel_uim_fkey_slot(code, mods, &store), i, n, sent = 0;
    n = sel_uim_fkey_lines(slot, store, lines);
    for (i = 0; send != NULL && i < n; i++) {
        sent += send(ctx, lines[i]) != 0;
    }
    return sent;
}

#include <selecta/uimodel/browse.h>

/* MUI-P8-3, P8-4: the Library mode Cycle and the Queue scope Cycle read the shared browse model. */
static uint32_t g_view_fires;

uint32_t mui_view_fires(void) { return g_view_fires; }

const char* mui_libmode_label(uint32_t i) { return sel_uim_libmode_name(i); }

uint32_t mui_libmode_fire(uint32_t i, uint32_t window_rows, mui_send_fn send, void* ctx) {
    char lines[SEL_UIM_LIB_LINES][SEL_UIM_LIB_LINE_MAX];
    uint32_t k, n = sel_uim_libmode_lines(i, window_rows, lines), sent = 0;
    g_view_fires++;
    for (k = 0; send != NULL && k < n; k++) {
        sent += send(ctx, lines[k]) != 0;
    }
    return sent;
}

const char* mui_scope_label(uint32_t i) { return sel_uim_scope_name(i); }

uint32_t mui_scope_fire(uint32_t i, mui_send_fn send, void* ctx) {
    char line[32];
    uint32_t sent = 0;
    g_view_fires++;
    if (sel_uim_scope_line(i, line, sizeof line) != 0u && send != NULL) {
        sent += send(ctx, line) != 0;
        sent += send(ctx, "VIEW_SOURCE queue") != 0; /* re-read the queue window */
    }
    return sent;
}

#include <selecta/uimodel/viewcmds.h>

/* MUI-P8-5 */
static const char* const k_viewbtn[2] = {"New Folder", "New Playlist"};

const char* mui_viewbtn_label(uint32_t i) {
    return i < MUI_VIEWBTN_COUNT ? (i < 2u ? k_viewbtn[i] : sel_uim_flag_label(i - 2u)) : NULL;
}

uint32_t mui_viewbtn_fire(uint32_t i, const char* text, uint32_t folder, mui_send_fn send, void* ctx) {
    char line[SEL_UIM_CMD_MAX];
    uint32_t n = i < 2u ? sel_uim_plbtn_line(i, text, folder, line, sizeof line) : sel_uim_flag_line(i - 2u, text, line, sizeof line);
    g_view_fires++;
    return n != 0u && send != NULL && send(ctx, line) != 0;
}

/* MUI-P8-12 */
uint32_t mui_tile_steps(void) { return SEL_UIM_TILE_STEPS; }
int32_t mui_tile_step(uint32_t i) { return sel_uim_tile_step(i); }

/* MUI-P8-14 */
static uint32_t g_pref_sets;

uint32_t mui_pref_sets(void) { return g_pref_sets; }

static uint32_t pref_send(uint32_t n, const char* line, mui_send_fn send, void* ctx) {
    g_pref_sets++;
    return n != 0u && send != NULL && send(ctx, line) != 0;
}

uint32_t mui_amiga_hotkeys_set(int enabled, mui_send_fn send, void* ctx) {
    char line[SEL_UIM_CMD_MAX];
    return pref_send(sel_uim_hotkeys_line(enabled, line, sizeof line), line, send, ctx);
}

uint32_t mui_amiga_priority_set(int32_t v, mui_send_fn send, void* ctx) {
    char line[SEL_UIM_CMD_MAX];
    return pref_send(sel_uim_priority_line(v, line, sizeof line), line, send, ctx);
}

uint32_t mui_amiga_group_set(const char* path, mui_send_fn send, void* ctx) {
    char line[SEL_UIM_CMD_MAX];
    return pref_send(sel_uim_groupfile_line(path, line, sizeof line), line, send, ctx);
}

/* MUI-P8-13 */
static uint32_t g_page_switches;

uint32_t mui_page_switches(void) { return g_page_switches; }

uint32_t mui_door_activate(uint32_t i, mui_send_fn send, void* ctx, const char** page) {
    const char* line = mui_door_line(i);
    const sel_uim_door* d = sel_uim_door_at(i);
    if (page != NULL) {
        *page = NULL;
    }
    if (d == NULL) {
        return 0;
    }
    if (line == NULL) {
        g_page_switches++;
        if (page != NULL) {
            *page = d->page;
        }
        return 0;
    }
    return send != NULL && send(ctx, line) != 0;
}

/* MUI-P8-6 */
static uint32_t g_stats_builds;

uint32_t mui_stats_builds(void) { return g_stats_builds; }

uint32_t mui_stats_text(const char* stats, int more, char* out, uint32_t cap) {
    uint32_t i, n = sel_uim_stats_shown(stats, more), len = 0;
    g_stats_builds++;
    if (cap == 0u) {
        return 0;
    }
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        const char *l, *v;
        int w;
        if (sel_uim_stats_row(stats, i, &l, &v) == 0u) {
            break;
        }
        w = snprintf(out + len, cap - len, "%s: %s\n", l, v);
        if (w < 0 || (uint32_t)w >= cap - len) {
            break;
        }
        len += (uint32_t)w;
    }
    return len;
}

#include <selecta/uimodel/panels.h>
#include <selecta/uimodel/sources.h>
#include <selecta/uimodel/viewcmds.h>

/* MUI-P8-7 */
static uint32_t g_panel_open, g_panel_builds;

uint32_t mui_panel_menu_id(uint32_t pane) { return pane < MUI_PANES ? MUI_ID_PANEL_BASE + pane : 0u; }

uint32_t mui_panel_key_pane(uint32_t code, uint32_t mods) { return code == 'a' && mods == TXT_MOD_CTRL ? SEL_UIM_PANEL_ACTIVITY : MUI_PANES; }

uint32_t mui_panel_toggle(uint32_t pane) {
    if (pane < MUI_PANES) {
        g_panel_open ^= 1u << pane;
    }
    return g_panel_open;
}

uint32_t mui_panel_open_mask(void) { return g_panel_open; }

uint32_t mui_panel_builds(void) { return g_panel_builds; }

uint32_t mui_panel_text(uint32_t pane, const sel_uim_net* st, char* out, uint32_t cap) {
    uint32_t i, n = sel_uim_panel_row_count(pane), used = 0;
    g_panel_builds++;
    if (cap > 0u) {
        out[0] = '\0';
    }
    for (i = 0; i < n; i++) {
        char row[96];
        uint32_t len = sel_uim_panel_row(pane, st, i, row, sizeof row);
        if (len != 0u && used + len + 2u <= cap) {
            memcpy(out + used, row, len);
            out[used + len] = '\n';
            out[used + len + 1u] = '\0';
            used += len + 1u;
        }
    }
    return used;
}

/* MUI-P8-8 */
static uint32_t g_source_fires;

uint32_t mui_source_fires(void) { return g_source_fires; }

static uint32_t send_line(uint32_t n, const char* line, mui_send_fn send, void* ctx) {
    return n != 0u && send != NULL && send(ctx, line) != 0;
}

uint32_t mui_source_toggle(const sel_uim_source* s, uint32_t n, uint32_t flip, mui_send_fn send, void* ctx) {
    char line[256];
    uint32_t sent = send_line(sel_uim_sources_off_line(s, n, flip, 0u, line, sizeof line), line, send, ctx);
    g_source_fires++;
    return sent + (send != NULL && send(ctx, "SOURCES") != 0);
}

uint32_t mui_source_sync(const sel_uim_source* s, mui_send_fn send, void* ctx) {
    char line[256];
    uint32_t sent = send_line(sel_uim_sync_line(s->key, s->path, line, sizeof line), line, send, ctx);
    g_source_fires++;
    return sent + (send != NULL && send(ctx, "SOURCES") != 0);
}

const char* mui_source_status(int pending, int failed, int64_t synced) { return sel_uim_sync_status(pending, failed, synced); }

uint32_t mui_offer_not_now(int64_t now, mui_send_fn send, void* ctx) {
    char line[96];
    uint32_t sent = send_line(sel_uim_sync_notnow_line(now, line, sizeof line), line, send, ctx);
    g_source_fires++;
    return sent + (send != NULL && send(ctx, "SOURCES") != 0);
}

uint32_t mui_offer_sync(const sel_uim_source* s, uint32_t n, uint32_t ticked, mui_send_fn send, void* ctx) {
    char line[256];
    uint32_t i, sent = 0;
    g_source_fires++;
    for (i = 0; i < n; i++) {
        if ((ticked & (1u << i)) != 0u) {
            sent += send_line(sel_uim_sync_line(s[i].key, s[i].path, line, sizeof line), line, send, ctx);
        }
    }
    return sent;
}

const char* mui_search_source_label(uint32_t i) { return sel_uim_search_source_name(i); }

uint32_t mui_search_source_set(uint32_t i, mui_send_fn send, void* ctx) {
    char line[96];
    g_source_fires++;
    return send_line(sel_uim_search_source_line(i, line, sizeof line), line, send, ctx);
}

/* MUI-P8-9 */
int32_t mui_info_mode_key(char key) { return sel_uim_info_mode_for_key(key); }
uint32_t mui_info_scroll_next(uint32_t offset, uint32_t text_len, int no_scroll) { return sel_uim_info_scroll_next(offset, text_len, no_scroll); }
const char* mui_info_ahi(int active) { return sel_uim_info_ahi_text(active); }
uint32_t mui_info_downloaded(const sel_uim_net* st, char* out, uint32_t cap) { return sel_uim_info_downloaded_text(st, out, cap); }
const char* mui_list_kept_cell(int kept) { return sel_uim_kept_marker(kept); }

/* MUI-P8-10 */
static uint32_t g_hovers;

uint32_t mui_tooltip_transport(uint32_t button, const sel_uim_frame* f, char* out, uint32_t cap) {
    const sel_uim_action* a = action_by_id(mui_transport_action(button, f));
    return a != NULL ? (uint32_t)sel_uim_action_tip_text(a, out, cap) : 0u;
}

uint32_t mui_tooltip_door(uint32_t i, char* out, uint32_t cap) {
    const sel_uim_door* d = sel_uim_door_at(i);
    return d != NULL ? (uint32_t)snprintf(out, cap, "%s", d->label) : 0u;
}

uint32_t mui_tooltip_hover(const char* tip, char* status, uint32_t cap) {
    g_hovers++;
    return (uint32_t)snprintf(status, cap, "%s", tip);
}

uint32_t mui_tooltip_hovers(void) { return g_hovers; }

/* MUI-P8-11 */
uint32_t mui_remember_close(const mui_remember* r, mui_send_fn send, void* ctx) {
    char value[48], line[SEL_UIM_CMD_MAX];
    uint32_t sent = 0;
    snprintf(value, sizeof value, "%d %d %u %u", (int)r->x, (int)r->y, (unsigned)r->w, (unsigned)r->h);
    sent += send_line(sel_uim_remember_line(SEL_UIM_REM_GEOMETRY, value, line, sizeof line), line, send, ctx);
    snprintf(value, sizeof value, "%u", (unsigned)r->panes);
    sent += send_line(sel_uim_remember_line(SEL_UIM_REM_PANES, value, line, sizeof line), line, send, ctx);
    sent += send_line(sel_uim_remember_line(SEL_UIM_REM_VISUALISER, r->viz, line, sizeof line), line, send, ctx);
    sent += send_line(sel_uim_remember_line(SEL_UIM_REM_BROWSE, r->browse, line, sizeof line), line, send, ctx);
    return sent;
}

uint32_t mui_remember_open(mui_remember* r, mui_pref_get_fn get, void* ctx) {
    const char* v;
    int x, y;
    unsigned w, h, panes;
    uint32_t found = 0;
    memset(r, 0, sizeof *r);
    v = get != NULL ? get(ctx, sel_uim_remember_key(SEL_UIM_REM_GEOMETRY)) : NULL;
    if (v != NULL && sscanf(v, "%d %d %u %u", &x, &y, &w, &h) == 4) {
        r->x = x;
        r->y = y;
        r->w = w;
        r->h = h;
        found++;
    }
    v = get != NULL ? get(ctx, sel_uim_remember_key(SEL_UIM_REM_PANES)) : NULL;
    if (v != NULL && sscanf(v, "%u", &panes) == 1) {
        r->panes = panes;
        found++;
    }
    v = get != NULL ? get(ctx, sel_uim_remember_key(SEL_UIM_REM_VISUALISER)) : NULL;
    if (v != NULL) {
        snprintf(r->viz, sizeof r->viz, "%s", v);
        found++;
    }
    v = get != NULL ? get(ctx, sel_uim_remember_key(SEL_UIM_REM_BROWSE)) : NULL;
    if (v != NULL) {
        snprintf(r->browse, sizeof r->browse, "%s", v);
        found++;
    }
    return found;
}
