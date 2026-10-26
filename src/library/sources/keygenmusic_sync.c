/* See keygenmusic_sync.h. No float, no allocation after the start (blocks from the host). */
#include "keygenmusic_sync.h"

#include "../local.h"
#include "../source.h"
#include "source/json_scan.h"
#include "sync_util.h"
#include "source/url_path.h"

#include <stdio.h>
#include <string.h>

#define SCAN_BATCH 32u
#define UNKNOWN_ARTIST "- unknown"

/* ---- the list ---- */

/* One object, its '{' consumed. Fills the wanted keys. Returns 0 on malformed input. */
static int read_object(FILE* f, sel_keygen_entry* e) {
    char key[16];
    int c;

    e->path[0] = e->group[0] = e->what[0] = '\0';
    for (;;) {
        c = sel_json_skip_ws(f);
        if (c == '}') {
            return 1;
        }
        if (c == ',') {
            continue;
        }
        if (c != '"' || !sel_json_read_string(f, key, sizeof key) || sel_json_skip_ws(f) != ':') {
            return 0;
        }
        c = sel_json_skip_ws(f);
        if (c == '"' && (strcmp(key, "path") == 0 || strcmp(key, "rg") == 0 || strcmp(key, "sn") == 0)) {
            char* dst = key[0] == 'p' ? e->path : key[0] == 'r' ? e->group : e->what;
            size_t n = key[0] == 'p' ? sizeof e->path : sizeof e->group;

            if (!sel_json_read_string(f, dst, n)) {
                return 0;
            }
            continue;
        }
        {
            int end = 0;

            if (!sel_json_skip_value(f, c, &end)) {
                return 0;
            }
            if (end == '}') {
                return 1;
            }
        }
    }
}

int32_t sel_keygen_read_list(FILE* f, int (*fn)(void* ctx, const sel_keygen_entry* e), void* ctx) {
    static sel_keygen_entry e; /* 900 bytes: static, not stack (one reader at a time) */
    int32_t n = 0;
    int c;

    if (f == NULL || sel_json_skip_ws(f) != '[') {
        return -1;
    }
    while ((c = sel_json_skip_ws(f)) != EOF && c != ']') {
        if (c == ',') {
            continue;
        }
        if (c != '{' || !read_object(f, &e)) {
            break;
        }
        n++;
        if (!fn(ctx, &e)) {
            break;
        }
    }
    return n;
}

/* ---- path, format, group ---- */

int sel_keygen_path(const char* listed, char* out, size_t n) {
    static const char root[] = "/" SEL_KEYGEN_ROOT;

    if (strncmp(listed, root, sizeof root - 1u) != 0) {
        return 0;
    }
    return sel_url_unquote(listed + sizeof root - 1u, out, n) && sel_sync_safe_path(out);
}

const char* sel_keygen_format(const char* path) {
    const char* file = strrchr(path, '/');
    const char* dot = strrchr(file != NULL ? file : path, '.');

    if (dot != NULL) {
        if (strcmp(dot, ".mod") == 0) {
            return "Protracker";
        }
        if (strcmp(dot, ".it") == 0) {
            return "Impulse Tracker";
        }
        if (strcmp(dot, ".s3m") == 0) {
            return "Scream Tracker 3";
        }
    }
    return "Fasttracker 2";
}

int sel_keygen_group(const char* path, const char* group, char* out, size_t n) {
    const char* from = group;
    const char* file = strchr(path, '/');
    size_t len;

    if (from == NULL || from[0] == '\0') {
        if (file != NULL && path[0] != '!') {
            from = path;
            len = (size_t)(file - path);
        } else {
            const char* sep;

            from = file != NULL ? file + 1 : path;
            sep = strstr(from, " - ");
            len = sep != NULL ? (size_t)(sep - from) : 0;
        }
    } else {
        len = strlen(from);
    }
    if (len == 0) {
        from = UNKNOWN_ARTIST;
        len = sizeof UNKNOWN_ARTIST - 1u;
    }
    if (len + 1u > n) {
        return 0;
    }
    memcpy(out, from, len);
    out[len] = '\0';
    return 1;
}

/* ---- the catalogue ---- */

typedef struct row_text {
    char path[SEL_KEYGEN_PATH_MAX];
    char artist[SEL_KEYGEN_TEXT_MAX];
    char name[SEL_KEYGEN_TEXT_MAX];
} row_text;

typedef struct sync_state {
    const sel_local_scan_config* cfg;
    sel_keygen_sync_stats* st;
    row_text* text;
    sel_scan_row* rows;
    uint32_t row_n;
    int failed;
} sync_state;

static int on_entry(void* ctx, const sel_keygen_entry* e) {
    sync_state* s = ctx;
    row_text* t = &s->text[s->row_n];
    sel_scan_row* r = &s->rows[s->row_n];

    s->st->entries++;
    if (!sel_keygen_path(e->path, t->path, sizeof t->path)) {
        s->st->skipped_rows++;
        return 1;
    }
    if (!sel_keygen_group(t->path, e->group, t->artist, sizeof t->artist)) {
        memcpy(t->artist, UNKNOWN_ARTIST, sizeof UNKNOWN_ARTIST);
    }
    if (e->what[0] != '\0') {
        memcpy(t->name, e->what, strlen(e->what) + 1u);
    } else { /* the file name without its extension */
        const char* file = strrchr(t->path, '/');
        const char* dot;
        size_t len;

        file = file != NULL ? file + 1 : t->path;
        dot = strrchr(file, '.');
        len = dot != NULL && dot != file ? (size_t)(dot - file) : strlen(file);
        if (len >= sizeof t->name) {
            len = sizeof t->name - 1u;
        }
        memcpy(t->name, file, len);
        t->name[len] = '\0';
    }
    memset(r, 0, sizeof *r);
    r->source = "keygenmusic";
    r->path = t->path;
    r->format = sel_keygen_format(t->path);
    r->artist = t->artist;
    r->album = r->md5 = "";
    r->name = t->name;
    r->size = 0; /* the list has no sizes */
    s->row_n++;
    s->st->listing_rows++;
    if (s->row_n == SCAN_BATCH &&
        !sel_sync_flush_scan(s->cfg, s->rows, &s->row_n, &s->st->batches, &s->failed)) {
        return 0;
    }
    return 1;
}

int sel_keygen_sync(const sel_local_scan_config* cfg, const char* dir, sel_keygen_sync_stats* stats) {
    sync_state s;
    const sel_host* host = cfg->host;
    char path[SEL_BROWSE_PATH_MAX];
    FILE* f = NULL;
    int ok = 0;

    memset(&s, 0, sizeof s);
    memset(stats, 0, sizeof *stats);
    s.cfg = cfg;
    s.st = stats;
    sel_source_register_builtin();
    s.text = host->mem_alloc(host, (size_t)SCAN_BATCH * sizeof *s.text, SEL_MEM_FAST);
    s.rows = host->mem_alloc(host, (size_t)SCAN_BATCH * sizeof *s.rows, SEL_MEM_FAST);
    if (s.text != NULL && s.rows != NULL && sel_source_find("keygenmusic") != NULL &&
        sel_browse_join(path, sizeof path, dir, SEL_KEYGEN_LIST)) {
        f = fopen(path, "rb");
    }
    if (f != NULL) {
        ok = sel_keygen_read_list(f, on_entry, &s) >= 0 && !s.failed &&
             sel_sync_flush_scan(cfg, s.rows, &s.row_n, &stats->batches, &s.failed);
        fclose(f);
    }
    if (s.text != NULL) {
        host->mem_free(host, s.text);
    }
    if (s.rows != NULL) {
        host->mem_free(host, s.rows);
    }
    return ok && !s.failed;
}
