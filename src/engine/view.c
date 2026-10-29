#include "view.h"

#include <string.h>

enum { KEY_FOLDER = 0, KEY_FORMAT = 1 };
static const char* const key_name[2] = {"folder", "format"};
static const uint8_t key_seq[2][2] = {{KEY_FOLDER, KEY_FOLDER}, {KEY_FORMAT, KEY_FOLDER}};
static const uint32_t key_len[2] = {1, 2}; /* columns before the songs */
static const char* const style_name[SEL_VIEW_STYLE_COUNT] = {"regular", "tree", "drilldown", "grid", "coverflow", "compact"};
#define OVERFLOW_GROUP "(more)"

/* The folders and home sources list prepared text rows (frows): always regular, cursor = row index. */
static int text_src(const sel_view* v) {
    return v->pub.source == SEL_VIEW_FOLDERS || v->pub.source == SEL_VIEW_HOME || v->pub.source == SEL_VIEW_SEARCH;
}

/* The browse source is a directory listing: always regular. */
static uint32_t mode_of(const sel_view* v) {
    /* Grid, cover flow and compact list the flat rows of the regular style, with artwork ids. */
    return v->pub.source != SEL_VIEW_BROWSE && !text_src(v) && v->pub.style <= SEL_VIEW_STYLE_DRILLDOWN ? v->pub.style : (uint32_t)SEL_VIEW_STYLE_REGULAR;
}

static uint32_t list_count(const sel_view* v, const sel_view_src* s) {
    if (mode_of(v) != SEL_VIEW_STYLE_REGULAR) {
        return v->total;
    }
    if (text_src(v)) {
        return v->frow_n;
    }
    return v->pub.source != SEL_VIEW_BROWSE ? s->queue->count : s->browse->count;
}

static void set_anchor(sel_view* v, const sel_view_src* s, uint32_t idx, uint32_t count);
static void build(sel_view* v, const sel_view_src* s);
static void anchor_styled(sel_view* v, const sel_view_src* s, uint32_t count);
static void fill_styled(sel_view* v, const sel_view_src* s);

static uint32_t row_id(const sel_view* v, const sel_view_src* s, uint32_t idx) {
    return v->pub.source != SEL_VIEW_BROWSE && !text_src(v) ? s->queue->entries[idx].id : idx;
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
    v->vote_text = host->mem_alloc(host, (size_t)row_cap * SEL_VIEW_VOTES_MAX, SEL_MEM_FAST);
    v->vote_id = host->mem_alloc(host, (size_t)row_cap * sizeof *v->vote_id, SEL_MEM_FAST);
    v->art_cache = host->mem_alloc(host, (size_t)row_cap * 2u * sizeof *v->art_cache, SEL_MEM_FAST);
    v->art_artist = host->mem_alloc(host, (size_t)row_cap * SEL_VIEW_ARTIST_MAX, SEL_MEM_FAST);
    if (v->vote_text == NULL || v->vote_id == NULL || v->art_cache == NULL || v->art_artist == NULL) {
        host->mem_free(host, v->art_artist);
        host->mem_free(host, v->art_cache);
        host->mem_free(host, v->vote_text);
        host->mem_free(host, v->vote_id);
        host->mem_free(host, v->row_text);
        host->mem_free(host, v->rows);
        v->rows = NULL;
        v->row_text = NULL;
        return 0;
    }
    memset(v->vote_id, 0, (size_t)row_cap * sizeof *v->vote_id);
    memset(v->art_cache, 0, (size_t)row_cap * 2u * sizeof *v->art_cache);
    memset(v->art_artist, 0, (size_t)row_cap * SEL_VIEW_ARTIST_MAX);
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
    v->fkey = host->mem_alloc(host, SEL_VIEW_HOME_TEXT_MAX, SEL_MEM_FAST);
    v->htext = host->mem_alloc(host, SEL_VIEW_HOME_TEXT_MAX, SEL_MEM_FAST);
    if (v->frows == NULL || v->ftext == NULL || v->fkey == NULL || v->htext == NULL) {
        host->mem_free(host, v->groups);
        host->mem_free(host, v->row_text);
        host->mem_free(host, v->rows);
        host->mem_free(host, v->frows);
        host->mem_free(host, v->ftext);
        host->mem_free(host, v->fkey);
        host->mem_free(host, v->htext);
        v->rows = NULL;
        v->row_text = NULL;
        v->groups = NULL;
        v->frows = NULL;
        v->ftext = NULL;
        v->fkey = NULL;
        v->htext = NULL;
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
    if (v->vote_text) {
        host->mem_free(host, v->vote_text);
    }
    if (v->art_cache) {
        host->mem_free(host, v->art_cache);
    }
    if (v->art_artist) {
        host->mem_free(host, v->art_artist);
    }
    if (v->vote_id) {
        host->mem_free(host, v->vote_id);
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
    if (v->fkey) {
        host->mem_free(host, v->fkey);
    }
    if (v->htext) {
        host->mem_free(host, v->htext);
    }
    memset(v, 0, sizeof *v);
}

/* ---- tree and drilldown ------------------------------------------------ */

static void queue_row_text(char* out, const sel_view_src* s, uint32_t idx, const char* votes);
static const char* row_votes(sel_view* v, const sel_view_src* s, uint32_t slot, uint32_t idx);
static uint32_t row_art(sel_view* v, const sel_view_src* s, uint32_t slot, uint32_t idx);

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
            r->artist = NULL;
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
        r->artist = NULL;
        r->depth = tree ? 1u : 0u;
        if (qi == SEL_QUEUE_NONE) {
            buf[0] = '\0';
            r->text = buf;
            continue;
        }
        r->id = s->queue->entries[qi].id;
        queue_row_text(buf, s, qi, row_votes(v, s, i, qi));
        r->text = buf;
    }
}

int sel_view_set_style(sel_view* v, const sel_view_src* s, const char* style) {
    uint32_t want;
    for (want = 0; want < SEL_VIEW_STYLE_COUNT && strcmp(style, style_name[want]) != 0; want++) {
    }
    if (want == SEL_VIEW_STYLE_COUNT) {
        return 0;
    }
    sel_view_refresh(v, s);
    if (want != v->pub.style) {
        anchor_to_song(v, s);
        v->pub.style = want;
        memset(v->art_cache, 0, (size_t)v->row_cap * 2u * sizeof *v->art_cache);
    }
    sel_view_refresh(v, s);
    return 1;
}

const char* sel_view_style_name(const sel_view* v) {
    return style_name[v->pub.style < SEL_VIEW_STYLE_COUNT ? v->pub.style : 0u];
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
    if (v->pub.style != SEL_VIEW_STYLE_TREE && v->pub.style != SEL_VIEW_STYLE_DRILLDOWN) {
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

/* Queue index of the song on display row `row`. Returns 0 for a tree or
 * drilldown node row, which names no song (I632b2). */
int sel_view_song_at(const sel_view* v, const sel_view_src* s, uint32_t row, uint32_t* queue_index) {
    int node;
    uint32_t id;
    if (mode_of(v) == SEL_VIEW_STYLE_REGULAR) {
        *queue_index = row;
        return row < s->queue->count;
    }
    row_at(v, s, row, &node, &id);
    if (node) {
        return 0;
    }
    *queue_index = sel_queue_index_of(s->queue, id);
    return *queue_index != SEL_QUEUE_NONE;
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
    } else if (strcmp(source, "home") == 0) {
        want = SEL_VIEW_HOME;
    } else if (strcmp(source, "search") == 0) {
        want = SEL_VIEW_SEARCH;
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
    if (text_src(v)) {
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

/* The vote count text of the row in window slot `slot` showing queue index `idx`: from the service callback, read
 * again only when the slot shows another entry or votes_generation moved (a row is drawn every pump, a sticker read
 * is a library call). NULL: none, or the title format has no %votes%. */
static const char* row_votes(sel_view* v, const sel_view_src* s, uint32_t slot, uint32_t idx) {
    char* out = v->vote_text + (size_t)slot * SEL_VIEW_VOTES_MAX;
    uint32_t id = s->queue->entries[idx].id;

    if (s->votes == NULL || s->title_format == NULL || strstr(s->title_format, "%votes%") == NULL) {
        return NULL;
    }
    if (v->vote_seen != s->votes_generation) {
        memset(v->vote_id, 0, (size_t)v->row_cap * sizeof *v->vote_id);
        v->vote_seen = s->votes_generation;
    }
    if (v->vote_id[slot] != id) {
        out[0] = '\0';
        if (!s->votes(s->votes_ctx, sel_queue_path(s->queue, idx), out)) {
            out[0] = '\0';
        }
        v->vote_id[slot] = id;
    }
    return out;
}

/* The artwork id of the row in window slot `slot` showing queue index `idx`: from the service callback, read again
 * only when the slot shows another entry or art_generation moved (a row is drawn every pump, a lookup is a library call). */
static uint32_t row_art(sel_view* v, const sel_view_src* s, uint32_t slot, uint32_t idx) {
    uint32_t* c = v->art_cache + (size_t)slot * 2u;
    uint32_t id = s->queue->entries[idx].id;

    if (s->art_id == NULL) {
        return 0;
    }
    if (v->art_seen != s->art_generation) {
        memset(v->art_cache, 0, (size_t)v->row_cap * 2u * sizeof *v->art_cache);
        v->art_seen = s->art_generation;
    }
    if (c[0] != id) {
        char* artist = v->art_artist + (size_t)slot * SEL_VIEW_ARTIST_MAX;
        c[1] = s->art_id(s->art_ctx, sel_queue_path(s->queue, idx));
        c[0] = id;
        artist[0] = '\0';
        if (c[1] != 0 && s->art_artist != NULL) {
            s->art_artist(s->art_ctx, c[1], artist, SEL_VIEW_ARTIST_MAX);
        }
    }
    return c[1];
}

/* A queue row's text: the title format over the fields a path alone gives
 * (no metadata store before M3): title and file are the basename, path the
 * whole path, format the lowercase extension. Other fields stay empty. */
static void queue_row_text(char* out, const sel_view_src* s, uint32_t idx, const char* votes) {
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
    fv.field[SEL_FMT_VOTES] = votes;
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
    {
        /* pub.version moves when the whole list changes identity or order; a cursor move or a scroll does not touch it. */
        uint32_t sig[8];
        sig[0] = p->source;
        sig[1] = p->style;
        sig[2] = count;
        sig[3] = s->queue->version;
        sig[4] = s->browse->generation;
        sig[5] = v->group_key;
        sig[6] = p->drill_n;
        sig[7] = v->fgen;
        if (memcmp(sig, v->vsig, sizeof sig) != 0) {
            memcpy(v->vsig, sig, sizeof sig);
            p->version++;
        }
    }
    for (i = 0; mode == SEL_VIEW_STYLE_REGULAR && i < p->row_n; i++) {
        uint32_t idx = p->window_start + i;
        sel_view_row* r = &v->rows[i];
        memset(r, 0, sizeof *r); /* every field fresh: a Files row must not keep the count of the page shown before */
        r->id = row_id(v, s, idx);
        if (text_src(v)) {
            *r = v->frows[idx];
        } else if (p->source != SEL_VIEW_BROWSE) {
            r->kind = SEL_ROW_QUEUE;
            char* buf = v->row_text + (size_t)i * SEL_VIEW_TEXT_MAX;
            queue_row_text(buf, s, idx, row_votes(v, s, i, idx));
            r->text = buf;
            if (v->pub.style >= SEL_VIEW_STYLE_GRID) {
                r->art_id = row_art(v, s, i, idx);
                if (v->art_artist[(size_t)i * SEL_VIEW_ARTIST_MAX] != '\0') {
                    r->artist = v->art_artist + (size_t)i * SEL_VIEW_ARTIST_MAX;
                }
            }
        } else {
            r->kind = s->browse->entries[idx].is_dir ? SEL_ROW_DIR : SEL_ROW_FILE;
            r->text = sel_browse_name(s->browse, idx);
        }
    }
    p->browse_path = s->browse->path;
    p->browse_leaf = sel_browse_basename(s->browse->path);
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
    now.votes_generation = s->votes_generation;
    now.art_generation = s->art_generation;
    changed = memcmp(&now, &v->last, sizeof now) != 0;
    *structure = v->force_changed || now.source != v->last.source || now.style != v->last.style ||
                 now.window_rows != v->last.window_rows || now.group_key != v->last.group_key;
    v->force_changed = 0;
    v->last = now;
    return changed || *structure;
}

const char* sel_view_style_label(uint32_t style) {
    return style < SEL_VIEW_STYLE_COUNT ? style_name[style] : "unknown";
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
            v->frows[n].key = NULL;
            v->frows[n].album = 0;
            v->frows[n].art_ref = NULL;
            v->frows[n].artist = NULL;
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

/* The home source (I191): parses the library's home text. A line is `H <heading>`, `Q <heading>` (the quick-pick grid, I478), `S <entry>` or
 * `C <count> <entry>`. Returns the number of entries the sections hold: one per `S` line and the count of a
 * `C` line (the home_rows sentinel). */
uint32_t sel_view_set_home(sel_view* v, const char* text) {
    uint32_t n = 0, entries = 0;
    size_t pool = 0; /* bytes of fkey used: keys are packed, a row costs its key length */
    const char* p = text;
    while (*p != '\0' && n < SEL_VIEW_FOLDER_ROWS) {
        char kind = *p;
        char* t = v->ftext + (size_t)n * SEL_VIEW_FOLDER_NAME;
        char* key = v->fkey + pool;
        size_t len = 0, klen = 0;
        uint32_t count = 1, album = 0, composer = 0, album_id = 0;
        char *art = NULL, *sart = NULL;
        key[0] = '\0';
        if ((kind == 'H' || kind == 'Q' || kind == 'S' || kind == 'C') && p[1] == ' ') {
            p += 2;
            if (kind == 'C') {
                count = 0;
                while (*p >= '0' && *p <= '9') {
                    count = count * 10u + (uint32_t)(*p++ - '0');
                }
                if (*p == ' ') {
                    p++;
                }
            } else if (kind == 'S') {
                const char* tab = p;
                while (*tab != '\0' && *tab != '\n' && *tab != '\t') {
                    tab++;
                }
                if (*tab == '\t') { /* "<key>\t<entry>"; a leading @ marks an album */
                    size_t raw, name;
                    const char *us, *end;
                    if (*p == '@') {
                        album = 1;
                        p++;
                    } else if (*p == '&') { /* I479: "&<canon>" is a composer tile; the key is the canon, there is no artwork to resolve */
                        composer = 1;
                        p++;
                    }
                    /* A key is never cut: one that does not fit the pool or the key limit leaves the row without a key.
                     * I471: an album key may carry its artist after a 0x1f byte ("<album>\x1f<artist>"); the row key is the album
                     * name alone and art_ref the whole string, so the artwork provider can tell same-named albums apart. */
                    {   /* I678h2: an album row ends its key with 0x1e and albums.id, so the tile opens the album page without matching names */
                        const char* idsep = p;
                        while (idsep < tab && *idsep != '\x1e') {
                            idsep++;
                        }
                        if (idsep < tab) {
                            const char* q = idsep + 1;
                            while (q < tab && *q >= '0' && *q <= '9') {
                                album_id = album_id * 10u + (uint32_t)(*q++ - '0');
                            }
                        }
                        end = idsep;
                    }
                    raw = name = (size_t)(end - p);
                    us = p;
                    while (us < end && *us != '\x1f') {
                        us++;
                    }
                    if (us < end) {
                        name = (size_t)(us - p); /* I482: a song key carries its artist after 0x1f too; the key is the part before it */
                    }
                    if (raw + 1u <= SEL_VIEW_KEY_MAX && pool + name + raw + 3u <= SEL_VIEW_HOME_TEXT_MAX) {
                        size_t i;
                        for (i = 0; i < name; i++) {
                            key[klen++] = p[i];
                        }
                        key[klen] = '\0';
                        art = key;
                        if (name != raw && !album) {
                            /* I482: a song row keeps the artist after its key, the key alone is the row key and art_ref. */
                            sart = key + klen + 1u;
                            memcpy(sart, p + name + 1u, raw - name - 1u);
                            sart[raw - name - 1u] = '\0';
                            pool += raw - name;
                        } else if (name != raw) {
                            art = key + klen + 1u;
                            memcpy(art, p, raw);
                            art[raw] = '\0';
                            pool += raw + 1u;
                        }
                        pool += klen + 1u;
                    }
                    p = tab + 1;
                }
            }
            while (*p != '\0' && *p != '\n' && len + 1u < SEL_VIEW_FOLDER_NAME) {
                t[len++] = *p++;
            }
            t[len] = '\0';
            v->frows[n].key = key[0] != '\0' ? key : NULL;
            v->frows[n].album = album;
            v->frows[n].art_id = album ? album_id : 0u;
            v->frows[n].composer = composer;
            v->frows[n].quick = kind == 'Q';
            v->frows[n].art_ref = key[0] != '\0' && !composer ? art : NULL;
            /* I476: an album tile's artist is the part of its art_ref after the 0x1f byte. */
            v->frows[n].artist = NULL;
            if (sart != NULL && sart[0] != '\0') {
                v->frows[n].artist = sart;
            }
            if (album && key[0] != '\0' && art != key && strchr(art, '\x1f') != NULL && strchr(art, '\x1f')[1] != '\0') {
                v->frows[n].artist = strchr(art, '\x1f') + 1;
            }
            v->frows[n].kind = kind == 'H' || kind == 'Q' ? SEL_ROW_HEADING : SEL_ROW_HOME;
            v->frows[n].id = n;
            v->frows[n].depth = kind == 'H' || kind == 'Q' ? 0u : 1u;
            v->frows[n].text = t;
            v->frows[n].count = 0;
            v->frows[n].expanded = 0;
            entries += kind == 'H' || kind == 'Q' ? 0u : count;
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
    return entries;
}
