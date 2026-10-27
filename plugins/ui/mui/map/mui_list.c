/* MUI list models from sel_view_state (plan 2026-10-25-amiga-mui.md MUI-P3-1..4, P3-7). Pure C. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

#include "mui_map.h"

static const mui_list_backend k_nlist = {"nlist", 1, 1};
static const mui_list_backend k_plain = {"plain", 0, 0};

const mui_list_backend* mui_list_backend_pick(int nlist_present, int listtree_present) {
    if (!nlist_present) {
        return &k_plain;
    }
    if (!listtree_present) { /* NList without Listtree: columns and sort, but the tree is indented text */
        static const mui_list_backend nlist_flat = {"nlist", 1, 0};
        return &nlist_flat;
    }
    return &k_nlist;
}

void mui_list_pending_arm(mui_list_pending* p, int32_t target, int32_t snapshot_active) {
    p->armed = 1;
    p->target = target;
    p->base = snapshot_active;
}

void mui_list_pending_clear(mui_list_pending* p) {
    p->armed = 0;
}

int32_t mui_list_pending_resolve(mui_list_pending* p, int32_t snapshot_active) {
    if (p->armed && snapshot_active == p->base) {
        return p->target;
    }
    p->armed = 0;
    return snapshot_active;
}

uint32_t mui_list_build(const mui_list_backend* be, mui_list_entry* out, uint32_t cap, const sel_view_state* v, int32_t* active) {
    uint32_t i, n = v->row_n;
    if (n > v->window_rows) {
        n = v->window_rows;
    }
    if (n > cap) {
        n = cap;
    }
    for (i = 0; i < n; i++) {
        const sel_view_row* r = &v->rows[i];
        mui_list_entry* e = &out[i];
        size_t used = 0;
        e->index = v->window_start + i;
        e->kind = r->kind;
        e->depth = r->depth;
        e->node = r->kind == SEL_ROW_NODE;
        e->expanded = e->node ? r->expanded : 0u;
        snprintf(e->label, sizeof e->label, "%s", r->text != NULL ? r->text : "");
        e->text[0] = '\0';
        if (!be->native_tree) {
            for (; used < 2u * r->depth && used + 1u < sizeof e->text; used++) {
                e->text[used] = ' ';
            }
            e->text[used] = '\0';
        }
        if (e->node && v->style == SEL_VIEW_STYLE_TREE) {
            snprintf(e->text + used, sizeof e->text - used, "%s %s (%u)", e->expanded ? "-" : "+", e->label, (unsigned)r->count);
        } else if (e->node) {
            snprintf(e->text + used, sizeof e->text - used, "%s (%u)", e->label, (unsigned)r->count);
        } else {
            snprintf(e->text + used, sizeof e->text - used, "%s", e->label);
        }
    }
    if (active != NULL) {
        *active = (v->cursor >= v->window_start && v->cursor - v->window_start < n) ? (int32_t)(v->cursor - v->window_start) : -1;
    }
    return n;
}

void mui_list_toggle_line(const sel_view_row* r, char* out, size_t cap) {
    if (r->kind != SEL_ROW_NODE) {
        snprintf(out, cap, "%s", "");
        return;
    }
    snprintf(out, cap, "%s %u", r->expanded ? "VIEW_COLLAPSE" : "VIEW_EXPAND", (unsigned)r->id);
}

void mui_list_breadcrumb(const sel_view_state* v, char* out, size_t cap) {
    sel_uim_queue_title(out, cap, v, 0);
}

uint32_t mui_columns_for(uint32_t breakpoint, uint8_t out[MUI_COL_COUNT]) {
    uint32_t n = 0;
    out[n++] = MUI_COL_TITLE;
    out[n++] = MUI_COL_ARTIST;
    if (breakpoint <= SEL_UIM_BP_ICON_SIDEBAR) {
        if (breakpoint <= SEL_UIM_BP_TWO_COLUMN) {
            out[n++] = MUI_COL_ALBUM;
        }
        out[n++] = MUI_COL_LENGTH;
        if (breakpoint == SEL_UIM_BP_THREE_COLUMN) {
            out[n++] = MUI_COL_FORMAT;
        }
    }
    return n;
}

void mui_columns_format(uint32_t breakpoint, char* out, size_t cap) {
    uint8_t cols[MUI_COL_COUNT];
    uint32_t i, n = mui_columns_for(breakpoint, cols);
    size_t used = 0;
    for (i = 0; i + 1u < n && used + 5u < cap; i++) {
        memcpy(out + used, "BAR,", 4);
        used += 4;
    }
    out[used] = '\0';
}

const char* mui_row_cell(const sel_view_row* r, uint32_t column) {
    switch (column) {
    case MUI_COL_TITLE:
        return r->text != NULL ? r->text : "";
    case MUI_COL_ARTIST:
        return r->artist != NULL ? r->artist : "";
    default:
        return "";
    }
}

void mui_row_extra_fill(const sel_state* st, const sel_view_row* r, const char* album, mui_row_extra* out) {
    const char* path = r->text != NULL ? r->text : "";
    const char* dot;
    const char* slash;
    uint32_t i;
    int playing = r->kind == SEL_ROW_QUEUE && st->song.queue_id != 0u && st->song.queue_id == r->id;
    out->album[0] = out->format[0] = '\0';
    if (r->kind == SEL_ROW_QUEUE && st->queue != NULL && st->queue_text != NULL) {
        for (i = 0; i < st->queue_count; i++) {
            if (st->queue[i].id == r->id) {
                path = st->queue_text + st->queue[i].path_offset;
                break;
            }
        }
    }
    dot = strrchr(path, '.');
    slash = strrchr(path, '/');
    if (slash == NULL) {
        slash = strrchr(path, ':');
    }
    if (dot != NULL && (slash == NULL || dot > slash) && strlen(dot + 1) < sizeof out->format) {
        strcpy(out->format, dot + 1);
    }
    if (album != NULL) {
        snprintf(out->album, sizeof out->album, "%s", album);
    }
    sel_uim_clock(out->length, sizeof out->length, playing ? st->song.duration_ms : 0u, playing && st->song.duration_ms != 0u);
}
