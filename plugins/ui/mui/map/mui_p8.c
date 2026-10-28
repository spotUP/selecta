/* MUI parity rows of plan 2026-10-25-amiga-mui.md section P8 (MUI-P8-1..15). Pure C, host-testable. Every action is read
 * from the shared action table (src/uimodel/actions.c) the desktop UI reads; the models are sel_uim_*. */
#include <selecta/uimodel.h>

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
