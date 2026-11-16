/* MUI list models from sel_view_state (plan 2026-10-25-amiga-mui.md MUI-P3-1..4, P3-7). Pure C. */
#include <selecta/uimodel.h>

#include <selecta/format_label.h>

#include <stdio.h>
#include <string.h>

#include "mui_aa.h"
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

int mui_list_dbl_race_on(const char* value) {
    return value != NULL && value[0] != '\0' && !(value[0] == '0' && value[1] == '\0');
}

uint32_t mui_list_dbl_row_calls;

int32_t mui_list_dbl_row(int32_t picked, int32_t active) {
    mui_list_dbl_row_calls++;
    return picked >= 0 ? picked : active;
}

/* I1406: the Show all (n) / Show less entry under a search category with more rows than the fold shows. index = the heading's window position. */
static void mui_fold_entry(mui_list_entry* e, const sel_view_state* v, uint32_t head, uint32_t rows) {
    int open = sel_uim_search_expanded(v->rows[head].text);
    memset(e, 0, sizeof *e);
    e->index = v->window_start + head;
    e->kind = MUI_ROW_FOLD;
    snprintf(e->label, sizeof e->label, "%s", open ? "Show less" : "Show all");
    if (open) {
        snprintf(e->text, sizeof e->text, "    Show less");
    } else {
        snprintf(e->text, sizeof e->text, "    Show all (%u)", (unsigned)rows);
    }
}

uint32_t mui_list_build(const mui_list_backend* be, mui_list_entry* out, uint32_t cap, const sel_view_state* v, int32_t* active) {
    uint32_t i, o = 0, n = v->row_n, head = 0, have_head = 0;
    int fold = v->source == SEL_VIEW_SEARCH && mui_style_kind(v->style) == MUI_STYLE_LIST;
    int32_t act = -1;
    if (n > v->window_rows) {
        n = v->window_rows;
    }
    for (i = 0; i <= n && o < cap; i++) {
        const sel_view_row* r = i < n ? &v->rows[i] : NULL;
        mui_list_entry* e;
        size_t used = 0;
        if (fold && have_head && (r == NULL || r->kind == (uint32_t)SEL_ROW_HEADING)) {
            uint32_t rows = sel_uim_search_section_rows(v->rows, v->row_n, head);
            if (rows > SEL_UIM_SEARCH_FOLD_ROWS) {
                mui_fold_entry(&out[o++], v, head, rows);
                if (o >= cap) {
                    break;
                }
            }
        }
        if (r == NULL) {
            break;
        }
        if (fold && r->kind == (uint32_t)SEL_ROW_HEADING) {
            head = i;
            have_head = 1;
        } else if (fold && sel_uim_search_row_folded(v->rows, v->row_n, i)) {
            continue;
        }
        e = &out[o];
        if (v->cursor == v->window_start + i) {
            act = (int32_t)o;
        }
        o++;
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
        } else if (v->source == SEL_VIEW_SEARCH && r->kind != (uint32_t)SEL_ROW_HEADING && r->artist != NULL && r->artist[0] != '\0') {
            snprintf(e->text + used, sizeof e->text - used, "%s" SEL_UIM_AUTHOR_SEP "%s", e->label, r->artist); /* I1412: the MUI list has one pen per line, so the author is not dimmed */
        } else {
            snprintf(e->text + used, sizeof e->text - used, "%s", e->label);
        }
    }
    if (active != NULL) {
        *active = act;
    }
    return o;
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

/* I637: the plain List sizes a column without W= to its widest cell (W=-1), so short names left the columns packed at the
 * left edge ("collapsed"). Title, Artist and Album carry weights and share the width the fixed Length / Format cells leave. */
static const int k_col_weight[MUI_COL_COUNT] = {40, 25, 25, 0, 0};

void mui_columns_format(uint32_t breakpoint, char* out, size_t cap, int aspect_q8) {
    uint8_t cols[MUI_COL_COUNT];
    uint32_t i, n = mui_columns_for(breakpoint, cols);
    size_t used = 0;
    mui_aa_list_t lm;
    mui_aa_list_metrics(13, aspect_q8, &lm); /* I667i: 8 px between a divider and the text on each side (design 3.6) */
    out[0] = '\0';
    for (i = 0; i < n; i++) {
        int w = k_col_weight[cols[i]];
        int last = i + 1u == n;
        int put;
        char ws[16];
        snprintf(ws, sizeof ws, "W=%d", w);
        if (last) {
            put = snprintf(out + used, cap - used, "%s", w > 0 ? ws : "");
        } else {
            put = snprintf(out + used, cap - used, "%s%sDELTA=%d BAR,", w > 0 ? ws : "", w > 0 ? " " : "", mui_aa_list_delta(&lm));
        }
        if (put < 0 || (size_t)put >= cap - used) {
            out[used] = '\0';
            return;
        }
        used += (size_t)put;
    }
}

uint32_t mui_columns_weights(uint32_t breakpoint, int weight_out[MUI_COL_COUNT]) {
    uint8_t cols[MUI_COL_COUNT];
    uint32_t i, n = mui_columns_for(breakpoint, cols);
    for (i = 0; i < n; i++) {
        weight_out[i] = k_col_weight[cols[i]];
    }
    return n;
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
    if (dot != NULL && (slash == NULL || dot > slash)) {
        sel_format_label(dot + 1, out->format, sizeof out->format); /* I819 */
    }
    if (album != NULL) {
        snprintf(out->album, sizeof out->album, "%s", album);
    }
    sel_uim_clock(out->length, sizeof out->length, playing ? st->song.duration_ms : 0u, playing && st->song.duration_ms != 0u);
}

uint32_t mui_text_lines(const char* text) {
    uint32_t n = 1, k;
    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    for (k = 0; text[k] != '\0'; k++) {
        n += text[k] == '\n';
    }
    return text[k - 1] == '\n' ? n - 1 : n;
}

void mui_text_line(const char* text, uint32_t row, char* out, size_t cap) {
    uint32_t r = 0;
    size_t len = 0;
    out[0] = '\0';
    if (text == NULL) {
        return;
    }
    for (; r < row; text++) {
        if (*text == '\0') {
            return;
        }
        r += *text == '\n';
    }
    while (text[len] != '\0' && text[len] != '\n') {
        len++;
    }
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(out, text, len);
    out[len] = '\0';
}
