/* See asma_sync.h. No float, no allocation after the start (blocks from the host). */
#include "asma_sync.h"

#include "../local.h"
#include "../source.h"
#include "sync_util.h"

#include <stdio.h>
#include <string.h>

#define SCAN_BATCH 32u
#define FORMAT_SAP "Slight Atari Player"
#define FORMAT_TTT "TIATracker"
#define UNKNOWN_ARTIST "- unknown"

/* ---- the list: a tiny streaming reader for the JSON inside asmadb.js ---- */

static int utf8_put(char* out, size_t n, size_t* used, unsigned long cp) {
    char b[4];
    size_t len, i;

    if (cp < 0x80u) {
        b[0] = (char)cp;
        len = 1;
    } else if (cp < 0x800u) {
        b[0] = (char)(0xC0u | (cp >> 6));
        b[1] = (char)(0x80u | (cp & 0x3Fu));
        len = 2;
    } else if (cp < 0x10000u) {
        b[0] = (char)(0xE0u | (cp >> 12));
        b[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[2] = (char)(0x80u | (cp & 0x3Fu));
        len = 3;
    } else {
        b[0] = (char)(0xF0u | (cp >> 18));
        b[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        b[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[3] = (char)(0x80u | (cp & 0x3Fu));
        len = 4;
    }
    if (out == NULL || *used + len + 1u > n) {
        return 0; /* cut: the rest of the string is read and dropped */
    }
    for (i = 0; i < len; i++) {
        out[(*used)++] = b[i];
    }
    return 1;
}

static int hex4(FILE* f, unsigned long* v) {
    int i;

    *v = 0;
    for (i = 0; i < 4; i++) {
        int c = getc(f);

        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            c = c - 'A' + 10;
        } else {
            return 0;
        }
        *v = *v * 16u + (unsigned long)c;
    }
    return 1;
}

/* Reads a string whose opening quote is consumed. Stores it (cut to `n` - 1 bytes) in `out` when not NULL.
 * Returns 0 on a malformed string or end of file. */
static int read_string(FILE* f, char* out, size_t n) {
    size_t used = 0;
    int c;

    if (out != NULL && n > 0) {
        out[0] = '\0';
    }
    while ((c = getc(f)) != EOF) {
        if (c == '"') {
            if (out != NULL && n > 0) {
                out[used] = '\0';
            }
            return 1;
        }
        if (c == '\\') {
            unsigned long cp;

            c = getc(f);
            switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'u':
                if (!hex4(f, &cp)) {
                    return 0;
                }
                if (cp >= 0xD800u && cp < 0xDC00u) { /* high surrogate: the low one follows as \uXXXX */
                    unsigned long lo;

                    if (getc(f) != '\\' || getc(f) != 'u' || !hex4(f, &lo)) {
                        return 0;
                    }
                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                }
                (void)utf8_put(out, n, &used, cp);
                continue;
            case EOF: return 0;
            default: break; /* \" \\ \/ stand for themselves */
            }
        }
        if (out != NULL && used + 2u <= n) {
            out[used++] = (char)c;
        }
    }
    return 0;
}

static int skip_ws(FILE* f) {
    int c;

    do {
        c = getc(f);
    } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
    return c;
}

/* Skips a value whose first byte `c` is read: a string, number or literal, or a nested object or array. Leaves the
 * delimiter that ends it (',' '}' ']') in `*end`. */
static int skip_value(FILE* f, int c, int* end) {
    int depth = 0;

    for (; c != EOF; c = getc(f)) {
        if (c == '"') {
            if (!read_string(f, NULL, 0)) {
                return 0;
            }
        } else if (c == '{' || c == '[') {
            depth++;
        } else if (c == '}' || c == ']') {
            if (depth == 0) {
                *end = c;
                return 1;
            }
            depth--;
        } else if (c == ',' && depth == 0) {
            *end = c;
            return 1;
        }
    }
    return 0;
}

/* One object, its '{' consumed. Fills the wanted keys. Returns 0 on malformed input. */
static int read_object(FILE* f, sel_asma_entry* e) {
    char key[24];
    int c;

    e->path[0] = e->title[0] = e->author[0] = '\0';
    e->size = -1;
    for (;;) {
        c = skip_ws(f);
        if (c == '}') {
            return 1;
        }
        if (c == ',') {
            continue;
        }
        if (c != '"' || !read_string(f, key, sizeof key) || skip_ws(f) != ':') {
            return 0;
        }
        c = skip_ws(f);
        if (c == '"' && (strcmp(key, "filePath") == 0 || strcmp(key, "title") == 0 || strcmp(key, "author") == 0)) {
            char* dst = key[0] == 'f' ? e->path : key[0] == 't' ? e->title : e->author;
            size_t n = key[0] == 'f' ? sizeof e->path : sizeof e->title;

            if (!read_string(f, dst, n)) {
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

            if (!skip_value(f, c, &end)) {
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
                if (skip_ws(f) != ':') {
                    at = 0;
                    continue;
                }
                return skip_ws(f) == '[';
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
    while ((c = skip_ws(f)) != EOF && c != ']') {
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

static int safe_path(const char* p) {
    return p[0] != '\0' && p[0] != '/' && strchr(p, '\\') == NULL && strstr(p, "..") == NULL;
}

static const char* format_of(const char* path) {
    const char* dot = strrchr(path, '.');

    return dot != NULL && strcmp(dot, ".ttt") == 0 ? FORMAT_TTT : FORMAT_SAP;
}

static int on_entry(void* ctx, const sel_asma_entry* e) {
    sync_state* s = ctx;
    row_text* t;
    sel_scan_row* r;

    s->st->entries++;
    if (!safe_path(e->path)) {
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
