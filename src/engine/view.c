#include "view.h"

#include <string.h>

static uint32_t list_count(const sel_view* v, const sel_view_src* s) {
    return v->pub.source == SEL_VIEW_QUEUE ? s->queue->count : s->browse->count;
}

static uint32_t row_id(const sel_view* v, const sel_view_src* s, uint32_t idx) {
    return v->pub.source == SEL_VIEW_QUEUE ? s->queue->entries[idx].id : idx;
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
    memset(v, 0, sizeof *v);
}

int sel_view_show(sel_view* v, const sel_view_src* s, const char* source) {
    uint32_t want;
    if (strcmp(source, "queue") == 0) {
        want = SEL_VIEW_QUEUE;
    } else if (strcmp(source, "browse") == 0) {
        want = SEL_VIEW_BROWSE;
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
    v->pub.cursor_id = count ? row_id(v, s, to) : 0;
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
    if (p->source == SEL_VIEW_QUEUE) {
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
    const char* dot = strrchr(base, '.');
    char ext[16];
    size_t n = 0;
    sel_format_values fv;
    memset(&fv, 0, sizeof fv);
    if (dot != NULL && dot != base) {
        for (dot++; *dot && n + 1u < sizeof ext; dot++, n++) {
            ext[n] = (*dot >= 'A' && *dot <= 'Z') ? (char)(*dot + ('a' - 'A')) : *dot;
        }
    }
    ext[n] = '\0';
    fv.field[SEL_FMT_TITLE] = base;
    fv.field[SEL_FMT_FILE] = base;
    fv.field[SEL_FMT_PATH] = path;
    fv.field[SEL_FMT_FORMAT] = ext;
    (void)sel_format_expand(s->title_format, &fv, SEL_VIEW_TITLE_DEFAULT, out, SEL_VIEW_TEXT_MAX);
}

void sel_view_refresh(sel_view* v, const sel_view_src* s) {
    sel_view_state* p = &v->pub;
    uint32_t count = list_count(v, s);
    uint32_t wr = p->window_rows, i;
    anchor(v, s, count);
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
    for (i = 0; i < p->row_n; i++) {
        uint32_t idx = p->window_start + i;
        sel_view_row* r = &v->rows[i];
        r->depth = 0;
        r->id = row_id(v, s, idx);
        if (p->source == SEL_VIEW_QUEUE) {
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
    changed = memcmp(&now, &v->last, sizeof now) != 0;
    *structure = v->force_changed || now.source != v->last.source || now.style != v->last.style ||
                 now.window_rows != v->last.window_rows;
    v->force_changed = 0;
    v->last = now;
    return changed || *structure;
}
