#include "view.h"

#include <string.h>

enum { KEY_FOLDER = 0, KEY_FORMAT = 1 };
static const char* const key_name[2] = {"folder", "format"};
static const uint8_t key_seq[2][2] = {{KEY_FOLDER, KEY_FOLDER}, {KEY_FORMAT, KEY_FOLDER}};
static const uint32_t key_len[2] = {1, 2}; /* columns before the songs */
static const char* const style_name[3] = {"regular", "tree", "drilldown"};
#define OVERFLOW_GROUP "(more)"

/* The browse source is a directory listing: always regular. */
static uint32_t mode_of(const sel_view* v) {
    return v->pub.source != SEL_VIEW_BROWSE && v->pub.source != SEL_VIEW_FOLDERS ? v->pub.style : (uint32_t)SEL_VIEW_STYLE_REGULAR;
}

static uint32_t list_count(const sel_view* v, const sel_view_src* s) {
    if (mode_of(v) != SEL_VIEW_STYLE_REGULAR) {
        return v->total;
    }
    if (v->pub.source == SEL_VIEW_FOLDERS) {
        return v->frow_n;
    }
    return v->pub.source != SEL_VIEW_BROWSE ? s->queue->count : s->browse->count;
}

static void set_anchor(sel_view* v, const sel_view_src* s, uint32_t idx, uint32_t count);
static void build(sel_view* v, const sel_view_src* s);
static void anchor_styled(sel_view* v, const sel_view_src* s, uint32_t count);
static void fill_styled(sel_view* v, const sel_view_src* s);

static uint32_t row_id(const sel_view* v, const sel_view_src* s, uint32_t idx) {
    return v->pub.source != SEL_VIEW_BROWSE && v->pub.source != SEL_VIEW_FOLDERS ? s->queue->entries[idx].id : idx;
}

int sel_view_init(sel_view* v, const sel_host* host, uint32_t row_cap) {
    memset(v, 0, sizeof *v);
    v->rows = host->mem_alloc(host, (size_t)row_cap * sizeof *v->rows, SEL_MEM_FAST);
    if (v->rows == NULL) {
        return 0;
    }
    v->row_text = host->mem_alloc(host, (size_t)row_cap * SEL_VIEW_TEXT_MAX, SEL_MEM_FAST);
    if (v->row_text == NULL) {
        host->mem_free(host, v->rows);
        v->rows = NULL;
        return 0;
    }
    v->groups = host->mem_alloc(host, SEL_VIEW_GROUPS_MAX * sizeof *v->groups, SEL_MEM_FAST);
    if (v->groups == NULL) {
        host->mem_free(host, v->row_text);
        host->mem_free(host, v->rows);
        v->rows = NULL;
        v->row_text = NULL;
        return 0;
    }
    v->frows = host->mem_alloc(host, SEL_VIEW_FOLDER_ROWS * sizeof *v->frows, SEL_MEM_FAST);
    v->ftext = host->mem_alloc(host, (size_t)SEL_VIEW_FOLDER_ROWS * SEL_VIEW_FOLDER_NAME, SEL_MEM_FAST);
    if (v->frows == NULL || v->ftext == NULL) {
        host->mem_free(host, v->groups);
        host->mem_free(host, v->row_text);
        host->mem_free(host, v->rows);
        host->mem_free(host, v->frows);
        host->mem_free(host, v->ftext);
        v->rows = NULL;
        v->row_text = NULL;
        v->groups = NULL;
        v->frows = NULL;
        v->ftext = NULL;
        return 0;
    }
    v->next_node = 1;
    v->row_cap = row_cap;
    v->pub.window_rows = row_cap < SEL_VIEW_WINDOW_DEFAULT ? row_cap : SEL_VIEW_WINDOW_DEFAULT;
    v->pub.rows = v->rows;
    return 1;
}

void sel_view_free(sel_view* v, const sel_host* host) {
    if (v->rows) {
        host->mem_free(host, v->rows);
    }
    if (v->row_text) {
        host->mem_free(host, v->row_text);
    }
    if (v->groups) {
        host->mem_free(host, v->groups);
    }
    if (v->frows) {
        host->mem_free(host, v->frows);
    }
    if (v->ftext) {
        host->mem_free(host, v->ftext);
    }
    memset(v, 0, sizeof *v);
}

/* ---- tree and drilldown ------------------------------------------------ */

static void queue_row_text(char* out, const sel_view_src* s, uint32_t idx);

/* Lowercase extension of a file name into `out`; empty when there is none. */
static void lower_ext(const char* base, char* out, size_t cap) {
    const char* dot = strrchr(base, '.');
    size_t n = 0;
    if (dot != NULL && dot != base) {
        for (dot++; *dot && n + 1u < cap; dot++, n++) {
            out[n] = (*dot >= 'A' && *dot <= 'Z') ? (char)(*dot + ('a' - 'A')) : *dot;
        }
    }
    out[n] = '\0';
}

/* The value of grouping key `kind` for a queue path, truncated to a group name. */
static void entry_key(uint32_t kind, const char* path, char* out) {
    if (kind == KEY_FOLDER) {
        const char* slash = strrchr(path, '/');
        size_t n;
        if (slash == NULL) {
            strcpy(out, ".");
            return;
        }
        n = slash == path ? 1u : (size_t)(slash - path);
        if (n >= SEL_VIEW_GROUP_NAME) {
            n = SEL_VIEW_GROUP_NAME - 1u;
        }
        memcpy(out, path, n);
        out[n] = '\0';
    } else {
        char ext[SEL_VIEW_GROUP_NAME];
        lower_ext(sel_browse_basename(path), ext, sizeof ext);
        strcpy(out, ext[0] ? ext : "(none)");
    }
}

static void copy_name(char* out, const char* in) {
    size_t n = strlen(in);
    if (n >= SEL_VIEW_GROUP_NAME) {
        n = SEL_VIEW_GROUP_NAME - 1u;
    }
    memcpy(out, in, n);
    out[n] = '\0';
}

/* Binary search in the name-sorted table; *pos is the match or the insert point. */
static int find_group(const sel_view* v, const char* name, uint32_t* pos) {
    uint32_t lo = 0, hi = v->ngroups;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        int c = strcmp(v->groups[mid].name, name);
        if (c == 0) {
            *pos = mid;
            return 1;
        }
        if (c < 0) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    *pos = lo;
    return 0;
}

static void reset_table(sel_view* v) {
    v->tbl_mode = 99u;
}

/* The key column the list shows: tree 0, drilldown the next one to pick. */
static uint32_t level_of(const sel_view* v) {
    return mode_of(v) == SEL_VIEW_STYLE_DRILLDOWN ? v->drill_n : 0u;
}

static int songs_col(const sel_view* v) {
    return mode_of(v) == SEL_VIEW_STYLE_DRILLDOWN && v->drill_n >= key_len[v->group_key];
}

/* Does queue entry `qi` match the values picked so far? */
static int passes(const sel_view* v, const sel_view_src* s, uint32_t qi) {
    uint32_t l, n = mode_of(v) == SEL_VIEW_STYLE_DRILLDOWN ? v->drill_n : 0u;
    char k[SEL_VIEW_GROUP_NAME];
    for (l = 0; l < n && l < 2u; l++) {
        entry_key(key_seq[v->group_key][l], sel_queue_path(s->queue, qi), k);
        if (strcmp(k, v->drill_value[l]) != 0) {
            return 0;
        }
    }
    return 1;
}

/* The group of entry `qi` in the current column; SEL_QUEUE_NONE when absent. */
static uint32_t group_of(const sel_view* v, const sel_view_src* s, uint32_t qi) {
    char k[SEL_VIEW_GROUP_NAME];
    uint32_t pos, lv = level_of(v);
    entry_key(key_seq[v->group_key][lv < 2u ? lv : 1u], sel_queue_path(s->queue, qi), k);
    if (find_group(v, k, &pos) || find_group(v, OVERFLOW_GROUP, &pos)) {
        return pos;
    }
    return SEL_QUEUE_NONE;
}

/* Finds or inserts the group for `key`; the last slot is the overflow group. */
static uint32_t group_slot(sel_view* v, const char* key) {
    uint32_t pos;
    if (find_group(v, key, &pos)) {
        return pos;
    }
    if (v->ngroups + 1u >= SEL_VIEW_GROUPS_MAX) {
        key = OVERFLOW_GROUP;
        if (find_group(v, key, &pos)) {
            return pos;
        }
    }
    memmove(&v->groups[pos + 1u], &v->groups[pos], (size_t)(v->ngroups - pos) * sizeof *v->groups);
    memset(&v->groups[pos], 0, sizeof v->groups[pos]);
    v->groups[pos].id = v->next_node++;
    copy_name(v->groups[pos].name, key);
    v->ngroups++;
    return pos;
}

/* Counts the groups of the current column and lays the rows out. */
static void build(sel_view* v, const sel_view_src* s) {
    uint32_t mode = mode_of(v), qi, g, out = 0, total = 0, matches = 0;
    int songs = songs_col(v);
    if (v->tbl_mode != mode) {
        v->ngroups = 0;
        v->next_node = 1;
        v->tbl_mode = mode;
    }
    for (g = 0; g < v->ngroups; g++) {
        v->groups[g].count = 0;
    }
    for (qi = 0; qi < s->queue->count; qi++) {
        char k[SEL_VIEW_GROUP_NAME];
        if (!passes(v, s, qi)) {
            continue;
        }
        matches++;
        if (!songs) {
            entry_key(key_seq[v->group_key][level_of(v)], sel_queue_path(s->queue, qi), k);
            v->groups[group_slot(v, k)].count++;
        }
    }
    for (g = 0; g < v->ngroups; g++) {
        if (v->groups[g].count != 0) {
            v->groups[out++] = v->groups[g];
        }
    }
    v->ngroups = out;
    if (songs) {
        total = matches;
    } else {
        for (g = 0; g < v->ngroups; g++) {
            v->groups[g].base = total;
            total += 1u + (mode == SEL_VIEW_STYLE_TREE && v->groups[g].expanded ? v->groups[g].count : 0u);
        }
    }
    v->total = total;
}

/* First entry at or after `from` that is in group `gi` (SEL_QUEUE_NONE: any
 * entry of the songs column). */
static uint32_t member_from(const sel_view* v, const sel_view_src* s, uint32_t gi, uint32_t from) {
    uint32_t qi;
    for (qi = from; qi < s->queue->count; qi++) {
        if (passes(v, s, qi) && (gi == SEL_QUEUE_NONE || group_of(v, s, qi) == gi)) {
            return qi;
        }
    }
    return SEL_QUEUE_NONE;
}

static uint32_t nth_member(const sel_view* v, const sel_view_src* s, uint32_t gi, uint32_t k) {
    uint32_t qi = member_from(v, s, gi, 0);
    while (k-- > 0 && qi != SEL_QUEUE_NONE) {
        qi = member_from(v, s, gi, qi + 1u);
    }
    return qi;
}

/* The group whose node row is at or before row `idx`. */
static uint32_t locate(const sel_view* v, uint32_t idx) {
    uint32_t g = 0;
    while (g + 1u < v->ngroups && v->groups[g + 1u].base <= idx) {
        g++;
    }
    return g;
}

/* What row `idx` of a tree or drilldown list is: a node (id = node id) or a song (id = queue id). */
static void row_at(const sel_view* v, const sel_view_src* s, uint32_t idx, int* node, uint32_t* id) {
    *node = 0;
    *id = 0;
    if (songs_col(v)) {
        uint32_t qi = nth_member(v, s, SEL_QUEUE_NONE, idx);
        *id = qi == SEL_QUEUE_NONE ? 0 : s->queue->entries[qi].id;
    } else if (v->ngroups) {
        uint32_t g = locate(v, idx);
        if (idx == v->groups[g].base) {
            *node = 1;
            *id = v->groups[g].id;
        } else {
            uint32_t qi = nth_member(v, s, g, idx - v->groups[g].base - 1u);
            *id = qi == SEL_QUEUE_NONE ? 0 : s->queue->entries[qi].id;
        }
    }
}

/* Makes row `idx` the row the cursor follows. */
static void set_anchor(sel_view* v, const sel_view_src* s, uint32_t idx, uint32_t count) {
    if (count == 0) {
        v->pub.cursor_id = 0;
        v->anchor_node = 0;
    } else if (mode_of(v) == SEL_VIEW_STYLE_REGULAR) {
        v->pub.cursor_id = row_id(v, s, idx);
    } else {
        row_at(v, s, idx, &v->anchor_node, &v->pub.cursor_id);
    }
}

/* Re-finds the cursor row after the list changed. A song hidden under a
 * collapsed node leaves the cursor on the node but keeps following the song. */
static void anchor_styled(sel_view* v, const sel_view_src* s, uint32_t count) {
    sel_view_state* p = &v->pub;
    uint32_t g;
    if (count == 0) {
        p->cursor = 0;
        return;
    }
    if (v->anchor_node) {
        int found = 0;
        for (g = 0; g < v->ngroups && !songs_col(v); g++) {
            if (v->groups[g].id == p->cursor_id) {
                p->cursor = v->groups[g].base;
                found = 1;
                break;
            }
        }
        if (!found) {
            p->cursor = p->cursor >= count ? count - 1u : p->cursor;
            set_anchor(v, s, p->cursor, count);
        }
    } else {
        uint32_t qi = sel_queue_index_of(s->queue, p->cursor_id);
        if (qi != SEL_QUEUE_NONE && passes(v, s, qi)) {
            uint32_t rank = 0, j;
            if (songs_col(v)) {
                for (j = 0; j < qi; j++) {
                    rank += passes(v, s, j) ? 1u : 0u;
                }
                p->cursor = rank;
            } else if ((g = group_of(v, s, qi)) != SEL_QUEUE_NONE) {
                p->cursor = v->groups[g].base;
                if (mode_of(v) == SEL_VIEW_STYLE_TREE && v->groups[g].expanded) {
                    for (j = 0; j < qi; j++) {
                        rank += passes(v, s, j) && group_of(v, s, j) == g ? 1u : 0u;
                    }
                    p->cursor += 1u + rank;
                }
            }
        } else {
            p->cursor = p->cursor >= count ? count - 1u : p->cursor;
            set_anchor(v, s, p->cursor, count);
        }
    }
    if (p->cursor >= count) {
        p->cursor = count - 1u;
    }
}

/* A cursor on a node moves to the node's first song before the list changes shape. */
static void anchor_to_song(sel_view* v, const sel_view_src* s) {
    if (v->anchor_node) {
        uint32_t g;
        for (g = 0; g < v->ngroups; g++) {
            if (v->groups[g].id == v->pub.cursor_id) {
                uint32_t qi = member_from(v, s, g, 0);
                v->anchor_node = 0;
                v->pub.cursor_id = qi == SEL_QUEUE_NONE ? 0 : s->queue->entries[qi].id;
                return;
            }
        }
        v->anchor_node = 0;
        v->pub.cursor_id = 0;
    }
}

/* Rows of the window in tree and drilldown: node rows, then songs. */
static void fill_styled(sel_view* v, const sel_view_src* s) {
    sel_view_state* p = &v->pub;
    uint32_t i, g = 0, qi = SEL_QUEUE_NONE, in_group = SEL_QUEUE_NONE;
    int songs = songs_col(v);
    int tree = mode_of(v) == SEL_VIEW_STYLE_TREE;
    for (i = 0; i < p->row_n; i++) {
        uint32_t idx = p->window_start + i;
        sel_view_row* r = &v->rows[i];
        char* buf = v->row_text + (size_t)i * SEL_VIEW_TEXT_MAX;
        memset(r, 0, sizeof *r);
        if (!songs) {
            g = locate(v, idx);
        }
        if (!songs && idx == v->groups[g].base) {
            r->kind = SEL_ROW_NODE;
            r->id = v->groups[g].id;
            r->text = v->groups[g].name;
            r->count = v->groups[g].count;
            r->expanded = tree && v->groups[g].expanded;
            in_group = SEL_QUEUE_NONE;
            continue;
        }
        if (in_group == (songs ? 0u : g) && qi != SEL_QUEUE_NONE) {
            qi = member_from(v, s, songs ? SEL_QUEUE_NONE : g, qi + 1u);
        } else {
            qi = nth_member(v, s, songs ? SEL_QUEUE_NONE : g, songs ? idx : idx - v->groups[g].base - 1u);
        }
        in_group = songs ? 0u : g;
        r->kind = SEL_ROW_QUEUE;
        r->depth = tree ? 1u : 0u;
        if (qi == SEL_QUEUE_NONE) {
            buf[0] = '\0';
            r->text = buf;
            continue;
        }
        r->id = s->queue->entries[qi].id;
        queue_row_text(buf, s, qi);
        r->text = buf;
    }
}

int sel_view_set_style(sel_view* v, const sel_view_src* s, const char* style) {
    uint32_t want;
    for (want = 0; want < 3u && strcmp(style, style_name[want]) != 0; want++) {
    }
    if (want == 3u) {
        return 0;
    }
    sel_view_refresh(v, s);
    if (want != v->pub.style) {
        anchor_to_song(v, s);
        v->pub.style = want;
    }
    sel_view_refresh(v, s);
    return 1;
}

const char* sel_view_style_name(const sel_view* v) {
    return style_name[v->pub.style < 3u ? v->pub.style : 0u];
}

int sel_view_set_group(sel_view* v, const sel_view_src* s, const char* key) {
    uint32_t k;
    for (k = 0; k < 2u && strcmp(key, key_name[k]) != 0; k++) {
    }
    if (k == 2u) {
        return 0;
    }
    sel_view_refresh(v, s);
    anchor_to_song(v, s);
    if (k != v->group_key) {
        v->group_key = k;
        v->drill_n = 0;
        reset_table(v);
    }
    if (v->pub.style == SEL_VIEW_STYLE_REGULAR) {
        v->pub.style = SEL_VIEW_STYLE_TREE;
    }
    sel_view_refresh(v, s);
    return 1;
}

int sel_view_expand(sel_view* v, const sel_view_src* s, uint32_t node, int on) {
    uint32_t g;
    sel_view_refresh(v, s);
    if (mode_of(v) != SEL_VIEW_STYLE_TREE) {
        return 0;
    }
    for (g = 0; g < v->ngroups; g++) {
        if (v->groups[g].id == node) {
            v->groups[g].expanded = on ? 1u : 0u;
            sel_view_refresh(v, s);
            return 1;
        }
    }
    return 0;
}

int sel_view_drill(sel_view* v, const sel_view_src* s, uint32_t level, const char* value) {
    uint32_t old = v->drill_n, pos;
    char name[SEL_VIEW_GROUP_NAME];
    sel_view_refresh(v, s);
    if (mode_of(v) != SEL_VIEW_STYLE_DRILLDOWN || level > v->drill_n || level >= key_len[v->group_key]) {
        return 0;
    }
    anchor_to_song(v, s);
    if (strcmp(value, "..") == 0) {
        v->drill_n = level;
    } else {
        copy_name(name, value);
        v->drill_n = level;
        reset_table(v);
        build(v, s);
        if (!find_group(v, name, &pos)) {
            v->drill_n = old;
            reset_table(v);
            sel_view_refresh(v, s);
            return 0;
        }
        memcpy(v->drill_value[level], name, sizeof name);
        v->drill_n = level + 1u;
    }
    reset_table(v);
    v->pub.cursor = 0;
    v->pub.window_start = 0;
    build(v, s);
    set_anchor(v, s, 0, v->total);
    sel_view_refresh(v, s);
    return 1;
}

/* Enter on the cursor row. Returns 1 with the queue index of a song to start;
 * 0 when the row was a node (tree: toggled, drilldown: drilled into). */
int sel_view_enter(sel_view* v, const sel_view_src* s, uint32_t* queue_index) {
    int node;
    uint32_t id;
    sel_view_refresh(v, s);
    if (v->pub.row_count == 0) {
        return 0;
    }
    if (mode_of(v) == SEL_VIEW_STYLE_REGULAR) {
        *queue_index = v->pub.cursor;
        return 1;
    }
    row_at(v, s, v->pub.cursor, &node, &id);
    if (!node) {
        *queue_index = sel_queue_index_of(s->queue, id);
        return *queue_index != SEL_QUEUE_NONE;
    }
    if (mode_of(v) == SEL_VIEW_STYLE_TREE) {
        uint32_t g = locate(v, v->pub.cursor);
        (void)sel_view_expand(v, s, id, !v->groups[g].expanded);
    } else {
        char name[SEL_VIEW_GROUP_NAME];
        copy_name(name, v->groups[locate(v, v->pub.cursor)].name);
        (void)sel_view_drill(v, s, v->drill_n, name);
    }
    return 0;
}

int sel_view_show(sel_view* v, const sel_view_src* s, const char* source) {
    uint32_t want;
    if (strcmp(source, "queue") == 0) {
        want = SEL_VIEW_QUEUE;
    } else if (strcmp(source, "query") == 0) {
        want = SEL_VIEW_QUERY;
    } else if (strcmp(source, "browse") == 0) {
        want = SEL_VIEW_BROWSE;
    } else if (strcmp(source, "folders") == 0) {
        want = SEL_VIEW_FOLDERS;
    } else {
        return 0;
    }
    if (want != v->pub.source) {
        v->cursor_of[v->pub.source] = v->pub.cursor;
        v->id_of[v->pub.source] = v->pub.cursor_id;
        v->pub.source = want;
        v->pub.cursor = v->cursor_of[want];
        v->pub.cursor_id = v->id_of[want];
        v->pub.window_start = 0;
    }
    sel_view_refresh(v, s);
    return 1;
}

/* Parses an unsigned decimal; 0 when `p` is not one. */
static int parse_uint(const char* p, uint32_t* out) {
    uint32_t n = 0;
    if (*p == '\0') {
        return 0;
    }
    for (; *p; p++) {
        if (*p < '0' || *p > '9' || n > (0xFFFFFFFFu - 9u) / 10u) {
            return 0;
        }
        n = n * 10u + (uint32_t)(*p - '0');
    }
    *out = n;
    return 1;
}

int sel_view_cursor(sel_view* v, const sel_view_src* s, const char* spec) {
    uint32_t count, n, to, last;
    sel_view_refresh(v, s);
    count = list_count(v, s);
    last = count ? count - 1u : 0;
    to = v->pub.cursor;
    if (strcmp(spec, "top") == 0) {
        to = 0;
    } else if (strcmp(spec, "bottom") == 0) {
        to = last;
    } else if (spec[0] == '+' && parse_uint(spec + 1, &n)) {
        to = n >= last || to + n >= last ? last : to + n;
    } else if (spec[0] == '-' && parse_uint(spec + 1, &n)) {
        to = n > to ? 0 : to - n;
    } else if (parse_uint(spec, &n)) {
        to = n > last ? last : n;
    } else {
        return 0;
    }
    v->pub.cursor = to;
    set_anchor(v, s, to, count);
    sel_view_refresh(v, s);
    return 1;
}

int sel_view_window(sel_view* v, const sel_view_src* s, uint32_t rows) {
    if (rows == 0 || rows > v->row_cap) {
        return 0;
    }
    v->pub.window_rows = rows;
    sel_view_refresh(v, s);
    return 1;
}

static void anchor(sel_view* v, const sel_view_src* s, uint32_t count) {
    sel_view_state* p = &v->pub;
    if (p->source == SEL_VIEW_FOLDERS) {
        /* The rows follow the tree order: the cursor is the row index. */
    } else if (p->source != SEL_VIEW_BROWSE) {
        uint32_t at = count ? sel_queue_index_of(s->queue, p->cursor_id) : SEL_QUEUE_NONE;
        if (at != SEL_QUEUE_NONE) {
            p->cursor = at;
        }
    } else if (s->browse->generation != v->browse_seen) {
        v->browse_seen = s->browse->generation;
        p->cursor = v->land;
        v->land = 0;
        p->window_start = 0;
    }
    if (p->cursor >= count) {
        p->cursor = count ? count - 1u : 0;
    }
    p->cursor_id = count ? row_id(v, s, p->cursor) : 0;
}

/* A queue row's text: the title format over the fields a path alone gives
 * (no metadata store before M3): title and file are the basename, path the
 * whole path, format the lowercase extension. Other fields stay empty. */
static void queue_row_text(char* out, const sel_view_src* s, uint32_t idx) {
    const char* path = sel_queue_path(s->queue, idx);
    const char* base = sel_browse_basename(path);
    char ext[16];
    sel_format_values fv;
    memset(&fv, 0, sizeof fv);
    lower_ext(base, ext, sizeof ext);
    fv.field[SEL_FMT_TITLE] = base;
    fv.field[SEL_FMT_FILE] = base;
    fv.field[SEL_FMT_PATH] = path;
    fv.field[SEL_FMT_FORMAT] = ext;
    if (s->meta != NULL && idx < s->meta_rows && s->meta_off[idx] != UINT32_MAX) {
        const char* m = s->meta + s->meta_off[idx];
        const char* artist = m + strlen(m) + 1u;
        const char* format = artist + strlen(artist) + 1u;
        const char* dur = format + strlen(format) + 1u;
        if (m[0] != '\0') {
            fv.field[SEL_FMT_TITLE] = m;
        }
        fv.field[SEL_FMT_AUTHOR] = artist;
        if (format[0] != '\0') {
            fv.field[SEL_FMT_FORMAT] = format;
        }
        fv.field[SEL_FMT_DURATION] = dur;
    }
    (void)sel_format_expand(s->title_format, &fv, SEL_VIEW_TITLE_DEFAULT, out, SEL_VIEW_TEXT_MAX);
}

void sel_view_refresh(sel_view* v, const sel_view_src* s) {
    sel_view_state* p = &v->pub;
    uint32_t mode = mode_of(v);
    uint32_t count, wr = p->window_rows, i;
    if (mode == SEL_VIEW_STYLE_REGULAR) {
        count = list_count(v, s);
        anchor(v, s, count);
    } else {
        build(v, s);
        count = v->total;
        anchor_styled(v, s, count);
    }
    if (p->cursor < p->window_start) {
        p->window_start = p->cursor;
    } else if (p->cursor >= p->window_start + wr) {
        p->window_start = p->cursor - wr + 1u;
    }
    if (p->window_start + wr > count) {
        p->window_start = count > wr ? count - wr : 0;
    }
    p->row_count = count;
    p->row_n = count - p->window_start < wr ? count - p->window_start : wr;
    if (mode != SEL_VIEW_STYLE_REGULAR) {
        fill_styled(v, s);
    }
    for (i = 0; mode == SEL_VIEW_STYLE_REGULAR && i < p->row_n; i++) {
        uint32_t idx = p->window_start + i;
        sel_view_row* r = &v->rows[i];
        r->depth = 0;
        r->id = row_id(v, s, idx);
        if (p->source == SEL_VIEW_FOLDERS) {
            *r = v->frows[idx];
        } else if (p->source != SEL_VIEW_BROWSE) {
            r->kind = SEL_ROW_QUEUE;
            char* buf = v->row_text + (size_t)i * SEL_VIEW_TEXT_MAX;
            queue_row_text(buf, s, idx);
            r->text = buf;
        } else {
            r->kind = s->browse->entries[idx].is_dir ? SEL_ROW_DIR : SEL_ROW_FILE;
            r->text = sel_browse_name(s->browse, idx);
        }
    }
    p->browse_path = s->browse->path;
    p->group = key_name[v->group_key];
    p->drill_n = mode == SEL_VIEW_STYLE_DRILLDOWN ? v->drill_n : 0u;
    p->drill[0] = v->drill_value[0];
    p->drill[1] = v->drill_value[1];
}

int sel_view_changed(sel_view* v, const sel_view_src* s, int* structure) {
    sel_view_sig now;
    int changed;
    if (!v->active) {
        return 0;
    }
    sel_view_refresh(v, s);
    memset(&now, 0, sizeof now);
    now.source = v->pub.source;
    now.style = v->pub.style;
    now.row_count = v->pub.row_count;
    now.cursor = v->pub.cursor;
    now.window_start = v->pub.window_start;
    now.window_rows = v->pub.window_rows;
    now.queue_version = s->queue->version;
    now.browse_generation = s->browse->generation;
    now.playing = s->playing;
    now.prefs_generation = s->prefs_generation;
    now.group_key = v->group_key;
    now.drill_n = v->pub.drill_n;
    now.folder_generation = v->fgen;
    changed = memcmp(&now, &v->last, sizeof now) != 0;
    *structure = v->force_changed || now.source != v->last.source || now.style != v->last.style ||
                 now.window_rows != v->last.window_rows || now.group_key != v->last.group_key;
    v->force_changed = 0;
    v->last = now;
    return changed || *structure;
}

const char* sel_view_style_label(uint32_t style) {
    return style < 3u ? style_name[style] : "unknown";
}

/* The folders source (I179): parses the FOLDER_TREE text. A line is `F <id> <name>` or `P <id> <name>`
 * after two spaces of indent per level. */
uint32_t sel_view_set_folders(sel_view* v, const char* tree) {
    uint32_t n = 0;
    const char* p = tree;
    while (*p != '\0' && n < SEL_VIEW_FOLDER_ROWS) {
        uint32_t depth = 0, id = 0;
        char kind;
        char* text = v->ftext + (size_t)n * SEL_VIEW_FOLDER_NAME;
        size_t len = 0;
        while (p[0] == ' ' && p[1] == ' ') {
            depth++;
            p += 2;
        }
        kind = *p;
        if ((kind == 'F' || kind == 'P') && p[1] == ' ') {
            p += 2;
            while (*p >= '0' && *p <= '9') {
                id = id * 10u + (uint32_t)(*p++ - '0');
            }
            if (*p == ' ') {
                p++;
            }
            while (*p != '\0' && *p != '\n' && len + 1u < SEL_VIEW_FOLDER_NAME) {
                text[len++] = *p++;
            }
            text[len] = '\0';
            v->frows[n].kind = kind == 'F' ? SEL_ROW_FOLDER : SEL_ROW_PLAYLIST;
            v->frows[n].id = id;
            v->frows[n].depth = depth;
            v->frows[n].text = text;
            v->frows[n].count = 0;
            v->frows[n].expanded = kind == 'F';
            n++;
        }
        while (*p != '\0' && *p != '\n') {
            p++;
        }
        if (*p == '\n') {
            p++;
        }
    }
    v->frow_n = n;
    v->fgen++;
    return n;
}
