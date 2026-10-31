/* I795 (drift doc P1, P4, P5): the page models of the MUI Home shelves and quick tiles, the Browse tiles and the Search facet chips.
 * Pure C, host tested. The splits follow the desktop UI (home_quick and home_shelves, browse_tiles, search_facet_name). */
#include "mui_map.h"

#include <selecta/uimodel.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mui_home_split(const sel_view_state* v, mui_home_page* out) {
    uint32_t k, in_quick = 0, last = 0, have = 0;
    memset(out, 0, sizeof *out);
    if (v == NULL || v->rows == NULL) {
        return;
    }
    for (k = 0; k < v->row_n; k++) {
        const sel_view_row* r = &v->rows[k];
        if (r->kind == (uint32_t)SEL_ROW_HEADING) {
            have = 0;
            if (r->quick) {
                out->quick_first = k + 1u;
                in_quick = 1;
            } else {
                in_quick = 0;
                if (out->shelf_n < MUI_HOME_SHELF_MAX) {
                    last = out->shelf_n++;
                    out->shelf[last].heading = k;
                    out->shelf[last].id = out->shelf_id + 1u;
                    out->shelf[last].first = k + 1u;
                    have = 1;
                }
                out->shelf_id++;
            }
        } else if (r->kind == (uint32_t)SEL_ROW_HOME) {
            if (in_quick) {
                out->quick_n++;
            } else if (have) {
                out->shelf[last].count++;
            }
        }
    }
    if (out->quick_n > MUI_HOME_QUICK_MAX) {
        out->quick_n = MUI_HOME_QUICK_MAX;
    }
}

uint32_t mui_shelf_find(const sel_view_state* v, uint32_t id, mui_shelf* out) {
    uint32_t k, seen = 0, quick = 0, on = 0;
    memset(out, 0, sizeof *out);
    if (v == NULL || v->rows == NULL) {
        return 0u;
    }
    for (k = 0; k < v->row_n; k++) {
        const sel_view_row* r = &v->rows[k];
        if (r->kind == (uint32_t)SEL_ROW_HEADING) {
            quick = r->quick ? 1u : 0u;
            on = 0;
            if (!quick && ++seen == id) {
                out->heading = k;
                out->first = k + 1u;
                out->id = id;
                on = 1;
            }
        } else if (r->kind == (uint32_t)SEL_ROW_HOME && on) {
            out->count++;
        }
    }
    return seen >= id && id != 0u ? 1u : 0u;
}

static const char* next_str(const char* p) { return p + strlen(p) + 1u; }

/* FORMATS_SHOW answers "format\0songs\0" per format, COMPOSERS_SHOW "canon\0display\0songs\0" per composer, each list ends with "\0". */
uint32_t mui_browse_tiles(const sel_state* st, mui_btile* out, uint32_t cap) {
    uint32_t n = 0, k;
    if (st != NULL && st->formats != NULL) {
        const char* p = st->formats;
        for (k = 0; *p != '\0' && n < cap && k < MUI_BROWSE_FORMATS; k++) {
            const char* cnt = next_str(p);
            uint32_t i;
            out[n].kind = MUI_TILE_FORMAT;
            out[n].count = (uint32_t)strtoul(cnt, NULL, 10);
            snprintf(out[n].label, sizeof out[n].label, "%s", p);
            snprintf(out[n].arg, sizeof out[n].arg, "%s", p);
            for (i = 0; out[n].arg[i] != '\0'; i++) { /* the query values are lower case */
                if (out[n].arg[i] >= 'A' && out[n].arg[i] <= 'Z') {
                    out[n].arg[i] = (char)(out[n].arg[i] + ('a' - 'A'));
                }
            }
            n++;
            p = next_str(cnt);
        }
    }
    if (st != NULL && st->composers != NULL) {
        const char* p = st->composers;
        for (k = 0; *p != '\0' && n < cap && k < MUI_BROWSE_COMPOSERS; k++) {
            const char* disp = next_str(p);
            const char* cnt = next_str(disp);
            out[n].kind = MUI_TILE_COMPOSER;
            out[n].count = (uint32_t)strtoul(cnt, NULL, 10);
            snprintf(out[n].label, sizeof out[n].label, "%s", disp);
            snprintf(out[n].arg, sizeof out[n].arg, "%s", p);
            n++;
            p = next_str(cnt);
        }
    }
    if (n < cap) { /* the Sources tile: the SOURCES list */
        out[n].kind = MUI_TILE_SOURCES;
        out[n].count = 0u;
        snprintf(out[n].label, sizeof out[n].label, "Sources");
        out[n].arg[0] = '\0';
        n++;
    }
    return n;
}

void mui_tile_caption(const mui_btile* t, char* out, size_t cap) {
    if (t->kind == MUI_TILE_SOURCES) {
        snprintf(out, cap, "%s", t->label);
    } else {
        snprintf(out, cap, "%s - %u song%s", t->label, (unsigned)t->count, t->count == 1u ? "" : "s");
    }
}

uint32_t mui_search_facet_line(const sel_view_state* v, uint32_t facet, char* out, size_t cap) {
    uint32_t k;
    if (facet >= SEL_UIM_SEARCH_FACETS) {
        return 0u;
    }
    if (facet == 0u) {
        snprintf(out, cap, "VIEW_CURSOR 0");
        return 1u;
    }
    if (v == NULL || v->rows == NULL) {
        return 0u;
    }
    for (k = 0; k < v->row_n; k++) { /* the group headings are "Songs (N)", "Albums (N)", "Artists (N)" */
        const sel_view_row* r = &v->rows[k];
        const char* name = sel_uim_search_facets[facet];
        if (r->kind == (uint32_t)SEL_ROW_HEADING && r->text != NULL && strncmp(r->text, name, strlen(name)) == 0) {
            snprintf(out, cap, "VIEW_CURSOR %u", (unsigned)(v->window_start + k));
            return 1u;
        }
    }
    return 0u;
}
