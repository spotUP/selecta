/* See asma_sync.h. No float, no allocation after the start (blocks from the host). */
#include "asma_sync.h"

#include "../local.h"
#include "../source.h"
#include "source/json_scan.h"
#include "sync_util.h"

#include <stdio.h>
#include <string.h>

#define SCAN_BATCH 32u
#define FORMAT_SAP "Slight Atari Player"
#define FORMAT_TTT "TIATracker"
#define UNKNOWN_ARTIST "- unknown"

/* ---- the list: the JSON inside asmadb.js, read with json_scan.h ---- */

/* One object, its '{' consumed. Fills the wanted keys. Returns 0 on malformed input. */
static int read_object(FILE* f, sel_asma_entry* e) {
    char key[24];
    int c;

    e->path[0] = e->title[0] = e->author[0] = '\0';
    e->size = -1;
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
        if (c == '"' && (strcmp(key, "filePath") == 0 || strcmp(key, "title") == 0 || strcmp(key, "author") == 0)) {
            char* dst = key[0] == 'f' ? e->path : key[0] == 't' ? e->title : e->author;
            size_t n = key[0] == 'f' ? sizeof e->path : sizeof e->title;

            if (!sel_json_read_string(f, dst, n)) {
                return 0;
            }
            continue;
        }
        if (strcmp(key, "fileSize") == 0 && c >= '0' && c <= '9') {
            int64_t v = 0;

            for (; c >= '0' && c <= '9'; c = getc(f)) {
                v = v * 10 + (c - '0');
            }
            e->size = v;
            if (c == '}') {
                return 1;
            }
            continue; /* c was ',' or whitespace */
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

/* Moves the stream to just after the '[' that follows the key "fileInfos". */
static int seek_file_infos(FILE* f) {
    static const char key[] = "\"fileInfos\"";
    size_t at = 0;
    int c;

    while ((c = getc(f)) != EOF) {
        if (c == key[at]) {
            at++;
            if (at == sizeof key - 1u) {
                if (sel_json_skip_ws(f) != ':') {
                    at = 0;
                    continue;
                }
                return sel_json_skip_ws(f) == '[';
            }
        } else {
            at = c == key[0] ? 1u : 0u;
        }
    }
    return 0;
}

int32_t sel_asma_read_list(FILE* f, int (*fn)(void* ctx, const sel_asma_entry* e), void* ctx) {
    static sel_asma_entry e; /* 900 bytes: static, not stack (one reader at a time) */
    int32_t n = 0;
    int c;

    if (f == NULL || !seek_file_infos(f)) {
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

/* ---- the artist ---- */

int sel_asma_artist(const char* author, char* out, size_t n) {
    size_t used = 0;
    const char* p = author != NULL ? author : "";

    if (n == 0) {
        return 0;
    }
    out[0] = '\0';
    while (*p != '\0') {
        const char* end = strstr(p, " & ");
        size_t len = end != NULL ? (size_t)(end - p) : strlen(p);
        char one[SEL_ASMA_TEXT_MAX];
        char* s;
        char* t;
        char* open;
        size_t m = 0, i;

        for (i = 0; i < len && m + 1u < sizeof one; i++) { /* drop every "<?>" */
            if (strncmp(p + i, "<?>", 3) == 0) {
                i += 2;
            } else {
                one[m++] = p[i];
            }
        }
        one[m] = '\0';
        s = one;
        while (*s == ' ') {
            s++;
        }
        t = s + strlen(s);
        while (t > s && t[-1] == ' ') {
            *--t = '\0';
        }
        if (t > s && t[-1] == ')') { /* trailing "(Handle)": the handle is the artist */
            open = t - 1;
            while (open > s && *open != '(') {
                open--;
            }
            if (*open == '(' && open + 1 < t - 1 && memchr(open + 1, ')', (size_t)(t - 1 - open - 1)) == NULL) {
                s = open + 1;
                t[-1] = '\0';
                t = s + strlen(s);
            }
        }
        while (*s == ' ') {
            s++;
        }
        for (t = s + strlen(s); t > s && t[-1] == ' ';) {
            *--t = '\0';
        }
        if (s[0] != '\0') {
            size_t l = strlen(s);

            if (used + l + (used > 0 ? 3u : 0u) + 1u > n) {
                return 0;
            }
            if (used > 0) {
                memcpy(out + used, " & ", 3);
                used += 3;
            }
            memcpy(out + used, s, l + 1u);
            used += l;
        }
        p = end != NULL ? end + 3 : p + len;
    }
    if (used == 0) {
        if (n < sizeof UNKNOWN_ARTIST) {
            return 0;
        }
        memcpy(out, UNKNOWN_ARTIST, sizeof UNKNOWN_ARTIST);
    }
    return 1;
}

/* ---- the catalogue ---- */

typedef struct row_text {
    char path[SEL_ASMA_PATH_MAX];
    char artist[SEL_ASMA_TEXT_MAX];
    char name[SEL_ASMA_TEXT_MAX];
} row_text;

typedef struct sync_state {
    const sel_local_scan_config* cfg;
    sel_asma_sync_stats* st;
    row_text* text;
    sel_scan_row* rows;
    uint32_t row_n;
    int failed;
} sync_state;

static const char* format_of(const char* path) {
    const char* dot = strrchr(path, '.');

    return dot != NULL && strcmp(dot, ".ttt") == 0 ? FORMAT_TTT : FORMAT_SAP;
}

static int on_entry(void* ctx, const sel_asma_entry* e) {
    sync_state* s = ctx;
    row_text* t;
    sel_scan_row* r;

    s->st->entries++;
    if (!sel_sync_safe_path(e->path)) {
        s->st->skipped_rows++;
        return 1;
    }
    t = &s->text[s->row_n];
    r = &s->rows[s->row_n];
    memcpy(t->path, e->path, strlen(e->path) + 1u);
    if (!sel_asma_artist(e->author, t->artist, sizeof t->artist)) {
        memcpy(t->artist, UNKNOWN_ARTIST, sizeof UNKNOWN_ARTIST);
    }
    if (e->title[0] != '\0') {
        memcpy(t->name, e->title, strlen(e->title) + 1u);
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
    r->source = "asma";
    r->path = t->path;
    r->format = format_of(t->path);
    r->artist = t->artist;
    r->album = r->md5 = "";
    r->name = t->name;
    r->size = e->size > 0 ? e->size : 0;
    s->row_n++;
    s->st->listing_rows++;
    if (s->row_n == SCAN_BATCH &&
        !sel_sync_flush_scan(s->cfg, s->rows, &s->row_n, &s->st->batches, &s->failed)) {
        return 0;
    }
    return 1;
}

int sel_asma_sync(const sel_local_scan_config* cfg, const char* dir, sel_asma_sync_stats* stats) {
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
    if (s.text != NULL && s.rows != NULL && sel_source_find("asma") != NULL &&
        sel_browse_join(path, sizeof path, dir, SEL_ASMA_LIST)) {
        f = fopen(path, "rb");
    }
    if (f != NULL) {
        ok = sel_asma_read_list(f, on_entry, &s) >= 0 && !s.failed &&
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
