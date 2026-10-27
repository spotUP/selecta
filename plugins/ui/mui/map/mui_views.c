/* MUI view mappings of Phase 4 (plan 2026-10-25-amiga-mui.md): pure C, host tested. */
#include "mui_map.h"

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
