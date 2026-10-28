/* MUI view mappings of Phase 4 (plan 2026-10-25-amiga-mui.md): pure C, host tested. */
#include "mui_map.h"

#include <selecta/uimodel/doors.h>

#include <stdio.h>
#include <string.h>

/* MUI-P4-1: what a click on a Home row does. The engine decides (VIEW_ACTIVATE plays a song, opens an album search or
 * opens a composer), so the lines are the same as the desktop shelf tile; the kind only picks the status word. */
mui_home_kind mui_home_kind_of(const sel_view_row* r) {
    if (r == NULL || r->kind != (uint32_t)SEL_ROW_HOME) {
        return MUI_HOME_NONE;
    }
    return r->composer ? MUI_HOME_COMPOSER : r->album ? MUI_HOME_ALBUM : MUI_HOME_SONG;
}

uint32_t mui_home_lines(const sel_view_state* v, uint32_t row, char lines[3][48]) {
    if (v == NULL || row >= v->row_n || mui_home_kind_of(&v->rows[row]) == MUI_HOME_NONE) {
        return 0;
    }
    snprintf(lines[0], 48, "VIEW_CURSOR %u", (unsigned)(v->window_start + row));
    snprintf(lines[1], 48, "VIEW_ACTIVATE");
    snprintf(lines[2], 48, "VIEW_SOURCE home");
    return 3;
}

/* MUI-P4-2: breadcrumb pieces of a browse path, split on "/" and on the AmigaDOS ":" (the volume keeps its colon). */
uint32_t mui_path_crumbs(const char* path, mui_crumb* out, uint32_t cap) {
    uint32_t n = 0;
    const char* p = path != NULL ? path : "";
    const char* start = p;
    for (;; p++) {
        if (*p == ':' || *p == '/' || *p == '\0') {
            size_t len = (size_t)(p - start) + (*p == ':' ? 1u : 0u);
            if (len > 0 && (*p != '/' || p > start)) {
                if (n < cap) {
                    if (len >= sizeof out[n].name) {
                        len = sizeof out[n].name - 1u;
                    }
                    memcpy(out[n].name, start, len);
                    out[n].name[len] = '\0';
                    out[n].end = (uint32_t)(p - path) + (*p == ':' ? 1u : 0u);
                }
                n++;
            }
            start = p + 1;
            if (*p == '\0') {
                break;
            }
        }
    }
    return n;
}

uint32_t mui_state_view_of(uint32_t source) {
    return source == SEL_VIEW_QUEUE ? SEL_UIM_SV_QUEUE : source == SEL_VIEW_QUERY || source == SEL_VIEW_SEARCH ? SEL_UIM_SV_SEARCH
           : source == SEL_VIEW_FOLDERS ? SEL_UIM_SV_PLAYLISTS : SEL_UIM_SV_LIBRARY;
}

/* MUI-P4-11: one status line, 3 s, errors 6 s; a new message replaces the visible one. */
uint32_t mui_toast_ms(int is_error) {
    return is_error ? 6000u : 3000u;
}

void mui_toast_show(mui_toast* t, const char* text, int is_error, uint32_t now_ms) {
    snprintf(t->text, sizeof t->text, "%s", text != NULL ? text : "");
    t->until_ms = now_ms + mui_toast_ms(is_error);
    t->visible = 1;
    t->shown++;
}

int mui_toast_tick(mui_toast* t, uint32_t now_ms) {
    if (t->visible && (int32_t)(now_ms - t->until_ms) >= 0) {
        t->visible = 0;
        t->text[0] = '\0';
        return 1;
    }
    return 0;
}

/* MUI-P4-14: one help row per action table row, the shared formatter. */
uint32_t mui_help_count(void) {
    return sel_uim_action_count();
}

void mui_help_row(uint32_t i, char* out, size_t cap) {
    sel_uim_help_row(i, out, cap);
}

/* MUI-P4-3: a changed search String gadget sends the one LIBRARY_SEARCH line, then selects the result view as the desktop does. */
uint32_t mui_search_lines(const char* text, char lines[3][160]) {
    if (sel_uim_search_line(text, lines[0], 160) == 0) {
        return 0;
    }
    snprintf(lines[1], 160, "VIEW_SOURCE search");
    snprintf(lines[2], 160, "VIEW_STYLE regular");
    return 3;
}

/* MUI-P4-4: the queue verbs take entry ids (the desktop sends the same). */
void mui_queue_line(const char* verb, uint32_t id, uint32_t to_id, char* out, size_t cap) {
    if (strcmp(verb, "move") == 0) {
        snprintf(out, cap, "QUEUE_MOVE %u %u", (unsigned)id, (unsigned)to_id);
    } else if (strcmp(verb, "remove") == 0) {
        snprintf(out, cap, "QUEUE_REMOVE %u", (unsigned)id);
    } else if (strcmp(verb, "clear") == 0) {
        snprintf(out, cap, "QUEUE_CLEAR");
    } else {
        out[0] = '\0';
    }
}

/* MUI-P4-5: playlist row actions. Open activates the row the list cursor is on (the list click sets VIEW_CURSOR first). */
uint32_t mui_playlist_line(const char* action, uint32_t id, const char* name, char* out, size_t cap) {
    if (strcmp(action, "open") == 0) {
        snprintf(out, cap, "VIEW_ACTIVATE");
    } else if (strcmp(action, "rename") == 0) {
        snprintf(out, cap, "PLAYLIST_RENAME %u \"%s\"", (unsigned)id, name != NULL ? name : "");
    } else if (strcmp(action, "delete") == 0) {
        snprintf(out, cap, "PLAYLIST_DEL %u", (unsigned)id);
    } else {
        return 0;
    }
    return 1;
}

/* MUI-P4-7: the rating Cycle holds None and 1..5 stars; picking an entry sets that rating. */
void mui_rating_line(uint32_t cycle_index, char* out, size_t cap) {
    snprintf(out, cap, "RATE %u", (unsigned)(cycle_index > 5u ? 5u : cycle_index));
}

const char* mui_like_line(void) {
    return "LIKE";
}

/* MUI-P4-8 */
uint32_t mui_np_objects(const char** names, uint32_t cap) {
    static const char* const objs[] = {"artwork", "title", "artist", "album", "lyrics", "upnext"};
    uint32_t i, n = (uint32_t)(sizeof objs / sizeof objs[0]);
    for (i = 0; i < n && i < cap; i++) {
        names[i] = objs[i];
    }
    return n;
}

int32_t mui_lyric_line_of(int32_t lyric_slot) {
    return lyric_slot > 0 ? lyric_slot - 1 : -1;
}

/* MUI-P4-9 */
uint32_t mui_subsong_cycle_line(uint32_t subsong, uint32_t count, char* out, size_t cap) {
    if (count < 2u) {
        return 0;
    }
    snprintf(out, cap, "SUBSONG %u", (unsigned)(subsong >= count ? 1u : subsong + 1u));
    return 1;
}

/* MUI-P4-12 */
void mui_confirm_text(uint32_t kind, const char* title, uint32_t songs, uint32_t playlists, char* out, size_t cap) {
    if (kind == 1u) {
        sel_uim_clear_text(songs, playlists, out, cap);
    } else {
        sel_uim_remove_text(title, out, cap);
    }
}

const char* mui_confirm_line(uint32_t kind) {
    return kind == 1u ? "LIBRARY_CLEAR" : NULL; /* a remove confirm sends LIBRARY_REMOVE with the path the caller holds */
}

/* MUI-P4-13 */
int mui_cmdline_submit(mui_send_fn send, void* ctx, const char* text) {
    return send != NULL && text != NULL && text[0] != '\0' ? send(ctx, text) : 0;
}

/* MUI-P4-16 */
void mui_tracker_cell_text(const sel_rp_cell* cell, char* out, size_t cap) {
    char note[8], ins[8], vol[8], fx[8];
    sel_uim_cell_fields(cell, note, ins, vol, fx);
    snprintf(out, cap, "%s %s %s %s", note, ins, vol, fx);
}

/* MUI-P4-0: sidebar doors. The page name picks the engine line the desktop page switch sends (desk_page_load). */
uint32_t mui_door_count(void) {
    return SEL_UIM_DOOR_COUNT;
}

const char* mui_door_label(uint32_t i) {
    const sel_uim_door* d = sel_uim_door_at(i);
    return d != NULL ? d->label : NULL;
}

const char* mui_door_line(uint32_t i) {
    static const struct {
        const char* page;
        const char* line;
    } k_line[] = {{"home", "VIEW_SOURCE home"},          {"search", "VIEW_SOURCE search"},     {"library", "VIEW_SHOW browse"},
                  {"playlists", "VIEW_SOURCE folders"},  {"queue", "VIEW_SHOW queue"}};
    const sel_uim_door* d = sel_uim_door_at(i);
    uint32_t k;
    for (k = 0; d != NULL && k < sizeof k_line / sizeof k_line[0]; k++) {
        if (strcmp(k_line[k].page, d->page) == 0) {
            return k_line[k].line;
        }
    }
    return NULL;
}
