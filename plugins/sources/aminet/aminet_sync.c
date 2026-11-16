/* See aminet_sync.h. No float, one block from the host. */
#include "aminet_sync.h"

#include "source/sync_text.h"
#include "source/xml_scan.h"

#include <selecta/catalogue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNKNOWN_ARTIST "- unknown"
#define LINE_MAX_BYTES 1100u

/* Next whitespace separated token of `*p`: returns its start and length, advances `*p` past it. NULL at the end. */
static const char* next_token(const char** p, size_t* len) {
    const char* s = *p;
    const char* t;

    while (*s == ' ' || *s == '\t') {
        s++;
    }
    if (*s == '\0') {
        *p = s;
        return NULL;
    }
    t = s;
    while (*s != '\0' && *s != ' ' && *s != '\t') {
        s++;
    }
    *len = (size_t)(s - t);
    *p = s;
    return t;
}

static int has_dir_prefix(const char* dir, size_t len) {
    return len >= 5u && memcmp(dir, "mods/", 5u) == 0;
}

/* Length of the extension (with the dot) when the name ends in an archive extension, else 0. */
static size_t archive_ext_len(const char* name, size_t len) {
    if (len > 4u && (memcmp(name + len - 4u, ".lha", 4u) == 0 || memcmp(name + len - 4u, ".lzh", 4u) == 0 ||
                     memcmp(name + len - 4u, ".zip", 4u) == 0)) {
        return 4u;
    }
    return 0;
}

/* "5K" -> 5120, "2.3M" -> 2411724 (one decimal, integer arithmetic), "712" -> 712. 0 when unreadable. */
static uint32_t parse_size(const char* s, size_t len) {
    uint64_t whole = 0, frac = 0, unit = 1;
    size_t i = 0;

    for (; i < len && s[i] >= '0' && s[i] <= '9'; i++) {
        whole = whole * 10u + (uint64_t)(s[i] - '0');
    }
    if (i < len && s[i] == '.') {
        i++;
        if (i < len && s[i] >= '0' && s[i] <= '9') {
            frac = (uint64_t)(s[i] - '0');
            i++;
        }
    }
    if (i < len) {
        unit = s[i] == 'K' ? 1024u : s[i] == 'M' ? 1024u * 1024u : s[i] == 'G' ? 1024u * 1024u * 1024u : 1u;
    }
    whole = whole * unit + frac * unit / 10u;
    return whole > 0xffffffffu ? 0xffffffffu : (uint32_t)whole;
}

int sel_aminet_parse_line(const char* line, sel_aminet_row* row) {
    const char* p = line;
    const char *name, *dir, *size, *age;
    size_t name_len = 0, dir_len = 0, size_len = 0, age_len = 0, ext, stem_len, n, desc_len;
    const char* by = NULL;

    if (line[0] == '|' || line[0] == '\0') {
        return 0;
    }
    name = next_token(&p, &name_len);
    dir = name != NULL ? next_token(&p, &dir_len) : NULL;
    size = dir != NULL ? next_token(&p, &size_len) : NULL;
    age = size != NULL ? next_token(&p, &age_len) : NULL;
    if (age == NULL || !has_dir_prefix(dir, dir_len) || (ext = archive_ext_len(name, name_len)) == 0) {
        return 0;
    }
    if (dir_len + 1u + name_len + 1u > sizeof row->path) {
        return 0;
    }
    memcpy(row->path, dir, dir_len);
    row->path[dir_len] = '/';
    memcpy(row->path + dir_len + 1u, name, name_len);
    row->path[dir_len + 1u + name_len] = '\0';
    if (!sel_sync_safe_path(row->path)) {
        return 0;
    }
    stem_len = name_len - ext;
    if (stem_len >= sizeof row->name) {
        stem_len = sizeof row->name - 1u;
    }
    memcpy(row->name, name, stem_len);
    row->name[stem_len] = '\0';
    row->size = parse_size(size, size_len);
    /* The artist: the text after the last " by " of the description, up to two spaces (the rating column follows) or a
     * " - " (I1411: "by Mortimer Twang - jazz", "by Nemesis - iCE & REM": the title or group follows). */
    desc_len = strlen(p) - strspn(p, " "); /* the description column starts after the gap */
    for (; (p = strstr(p, " by ")) != NULL; p += 4) {
        by = p + 4;
    }
    n = 0;
    if (by != NULL) {
        while (by[n] != '\0' && !(by[n] == ' ' && (by[n + 1u] == ' ' || by[n + 1u] == '\0')) &&
               strncmp(by + n, " - ", 3) != 0) {
            n++;
        }
    }
    /* I1411: an artist running into the end of a full description column is cut ("Mortimer Tw"); the sync resolves it. */
    row->cut = (uint8_t)(n > 0 && by[n] == '\0' && desc_len >= SEL_AMINET_DESC_COLS);
    if (n == 0 || n >= sizeof row->artist) {
        row->cut = 0;
        memcpy(row->artist, UNKNOWN_ARTIST, sizeof UNKNOWN_ARTIST);
    } else {
        memcpy(row->artist, by, n);
        row->artist[n] = '\0';
    }
    return 1;
}

/* I1411: rows whose artist may be cut ("Mortimer Tw"), held back until a second read of the INDEX finds the one longer
 * artist each prefixes. shortcut: rows past CUT_ROWS or the arena keep the cut name; raise both if Aminet outgrows them. */
#define CUT_ROWS 2048u
#define CUT_ARENA 131072u

typedef struct cut_row {
    const char* path;
    const char* name;
    const char* artist;
    const char* full; /* the longer artist it prefixes; NULL none yet, AMBIGUOUS when two differ */
    uint32_t size;
} cut_row;

typedef struct cut_set {
    cut_row rows[CUT_ROWS];
    uint32_t count, used;
    char arena[CUT_ARENA];
} cut_set;

static const char AMBIGUOUS[] = "";

typedef struct sync_state {
    const sel_host* host;
    const sel_catalogue* cat;
    sel_aminet_sync_stats* st;
    sel_aminet_row* row;
    cut_set* cut;
    int resolving; /* second read: match full artists against the held rows, emit nothing */
    int failed;
} sync_state;

static const char* cut_copy(cut_set* c, const char* text) {
    size_t n = strlen(text) + 1u;
    char* p;

    if (n > CUT_ARENA - c->used) {
        return NULL;
    }
    p = c->arena + c->used;
    memcpy(p, text, n);
    c->used += (uint32_t)n;
    return p;
}

/* Orders a held artist against the first k bytes of `a`; 0 when they are equal. */
static int cut_cmp(const char* held, const char* a, size_t k) {
    int r = strncmp(held, a, k);

    return r != 0 ? r : held[k] != '\0';
}

static int cut_sort(const void* x, const void* y) {
    return strcmp(((const cut_row*)x)->artist, ((const cut_row*)y)->artist);
}

/* Every held row whose artist is a strict prefix of the full artist `a` takes it, unless another full artist came first. */
static void cut_match(cut_set* c, const char* a) {
    size_t k, len = strlen(a);

    for (k = 1; k < len; k++) {
        uint32_t lo = 0, hi = c->count;

        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2u;

            if (cut_cmp(c->rows[mid].artist, a, k) < 0) {
                lo = mid + 1u;
            } else {
                hi = mid;
            }
        }
        for (; lo < c->count && cut_cmp(c->rows[lo].artist, a, k) == 0; lo++) {
            cut_row* r = &c->rows[lo];

            if (r->full == NULL) {
                r->full = cut_copy(c, a);
                r->full = r->full != NULL ? r->full : AMBIGUOUS;
            } else if (r->full != AMBIGUOUS && strcmp(r->full, a) != 0) {
                r->full = AMBIGUOUS;
            }
        }
    }
}

static int emit(sync_state* s, const char* path, const char* name, const char* artist, uint32_t size) {
    sel_catalogue_song song;

    memset(&song, 0, sizeof song);
    song.path = path;
    song.format = SEL_AMINET_FORMAT;
    song.artist = artist;
    song.album = song.md5 = "";
    song.name = name;
    song.size = size;
    if (!s->cat->song(s->host, &song)) {
        s->failed = 1;
        return 0;
    }
    return 1;
}

static int on_line(sync_state* s, const char* line) {
    sel_aminet_row* r = s->row;

    if (line[0] == '|' || line[0] == '\0') {
        return 1;
    }
    if (s->resolving) {
        if (sel_aminet_parse_line(line, r) && !r->cut) {
            cut_match(s->cut, r->artist);
        }
        return 1;
    }
    s->st->lines++;
    s->st->parse_calls++;
    if (!sel_aminet_parse_line(line, r)) {
        s->st->skipped_rows++;
        return 1;
    }
    s->st->listing_rows++;
    if (r->cut && s->cut != NULL && s->cut->count < CUT_ROWS) {
        cut_set* c = s->cut;
        uint32_t used = c->used;
        cut_row* h = &c->rows[c->count];

        h->path = cut_copy(c, r->path);
        h->name = h->path != NULL ? cut_copy(c, r->name) : NULL;
        h->artist = h->name != NULL ? cut_copy(c, r->artist) : NULL;
        h->full = NULL;
        h->size = r->size;
        if (h->artist != NULL) {
            c->count++;
            return 1;
        }
        c->used = used; /* arena full: emit it as read */
    }
    return emit(s, r->path, r->name, r->artist, r->size);
}

/* After the first read: sort the held rows, read the INDEX again for full artists, emit the held rows resolved. */
static int resolve_cut(FILE* f, sync_state* s);

/* Reads `f` line by line; a line longer than the buffer is cut (its tail is the description) and the rest dropped. */
static int read_index(FILE* f, sync_state* s) {
    char line[LINE_MAX_BYTES];

    while (fgets(line, sizeof line, f) != NULL) {
        size_t n = strlen(line);

        if (n > 0 && line[n - 1u] != '\n' && !feof(f)) {
            int c;

            while ((c = fgetc(f)) != EOF && c != '\n') {
            }
        }
        while (n > 0 && (line[n - 1u] == '\n' || line[n - 1u] == '\r')) {
            line[--n] = '\0';
        }
        if (!on_line(s, line)) {
            return 0;
        }
    }
    return !ferror(f);
}

static int resolve_cut(FILE* f, sync_state* s) {
    cut_set* c = s->cut;
    uint32_t i;

    if (c == NULL || c->count == 0) {
        return 1;
    }
    qsort(c->rows, c->count, sizeof c->rows[0], cut_sort);
    rewind(f);
    s->resolving = 1;
    if (!read_index(f, s)) {
        return 0;
    }
    for (i = 0; i < c->count; i++) {
        const cut_row* r = &c->rows[i];

        if (!emit(s, r->path, r->name, r->full != NULL && r->full != AMBIGUOUS ? r->full : r->artist, r->size)) {
            return 0;
        }
    }
    return 1;
}

int sel_aminet_sync(const sel_host* host, const char* dir, sel_aminet_sync_stats* stats) {
    sync_state s;
    const sel_catalogue* cat = host->catalogue != NULL ? host->catalogue(host) : NULL;
    char path[1100];
    FILE* f = NULL;
    int ok = 0, open = 0;

    memset(&s, 0, sizeof s);
    memset(stats, 0, sizeof *stats);
    s.host = host;
    s.cat = cat;
    s.st = stats;
    s.row = host->mem_alloc(host, sizeof *s.row, SEL_MEM_FAST);
    if (s.row != NULL && cat != NULL && snprintf(path, sizeof path, "%s/%s", dir, SEL_AMINET_LIST) < (int)sizeof path &&
        (f = fopen(path, "rb")) != NULL) {
        open = cat->begin(host, "aminet");
        if (open) {
            s.cut = host->mem_alloc(host, sizeof *s.cut, SEL_MEM_FAST);
            if (s.cut != NULL) {
                s.cut->count = s.cut->used = 0;
            }
            ok = read_index(f, &s) && !s.failed && resolve_cut(f, &s);
            if (s.cut != NULL) {
                host->mem_free(host, s.cut);
            }
        }
        fclose(f);
    }
    if (open) {
        if (ok) {
            ok = cat->commit(host);
        } else {
            cat->abort(host);
        }
    }
    if (s.row != NULL) {
        host->mem_free(host, s.row);
    }
    return ok;
}

/* ---- search answer (I1024g) ---- */

#define SEARCH_PAGE_MAX (512u * 1024u)
#define CELL_BYTES 256u

/* Copies `src[0..len)` into `out` without markup and with the HTML entities of the site decoded. */
static void cell_text(const char* src, size_t len, char* out, size_t n) {
    size_t i = 0, o = 0;
    int tag = 0;

    for (; i < len && o + 1u < n; i++) {
        size_t used = 0, k;

        if (src[i] == '<') {
            tag = 1;
        } else if (src[i] == '>') {
            tag = 0;
        } else if (!tag) {
            k = src[i] == '&' ? sel_xml_entity(src + i, src + len, out + o, n - o - 1u, &used) : 0;
            if (k == 2 && (unsigned char)out[o] == 0xC2u && (unsigned char)out[o + 1] == 0xA0u) {
                out[o] = ' '; /* "&nbsp;" stays a plain space here */
                k = 1;
            }
            if (k > 0) {
                o += k;
                i += used - 1u;
            } else {
                out[o++] = src[i];
            }
        }
    }
    out[o] = '\0';
}

/* The next `<a ...>text</a>` of [*p, end): its href start (NULL when none) and its text; advances `*p` past `</a>`. */
static int next_anchor(const char** p, const char* end, const char** href, char* text, size_t n) {
    const char* a = strstr(*p, "<a ");
    const char *gt, *close;

    if (a == NULL || a >= end || (gt = strchr(a, '>')) == NULL || (close = strstr(gt, "</a>")) == NULL || close > end) {
        return 0;
    }
    *href = strstr(a, "href=\"");
    *href = *href != NULL && *href < gt ? *href + 6 : NULL;
    cell_text(gt + 1, (size_t)(close - gt - 1), text, n);
    *p = close + 4;
    return 1;
}

typedef struct search_state {
    const sel_search_sink* sink;
    sel_aminet_row* row;
    int32_t given;
} search_state;

/* One `<tr>`: name, directory, size column and the package description anchor, as an INDEX line. */
static int search_row(search_state* s, const char* from, const char* end) {
    char name[CELL_BYTES], dir[CELL_BYTES], size[32], desc[CELL_BYTES], text[CELL_BYTES], line[LINE_MAX_BYTES];
    const char *p = from, *href, *sc;

    name[0] = dir[0] = desc[0] = size[0] = '\0';
    if (!next_anchor(&p, end, &href, name, sizeof name) || !next_anchor(&p, end, &href, dir, sizeof dir)) {
        return 1;
    }
    sc = strstr(p, "class=\"size_col\"");
    sc = sc != NULL && sc < end ? strchr(sc, '>') : NULL;
    sc = sc != NULL ? strchr(sc + 1, '>') : NULL; /* the <font> inside the cell */
    if (sc != NULL && sc < end) {
        const char* e = strchr(sc + 1, '<');

        cell_text(sc + 1, e != NULL ? (size_t)(e - sc - 1) : 0, size, sizeof size);
    }
    while (next_anchor(&p, end, &href, text, sizeof text)) {
        if (href != NULL && strncmp(href, "/package/", 9u) == 0) {
            memcpy(desc, text, sizeof desc);
            break;
        }
    }
    snprintf(line, sizeof line, "%s %s %s 0 %s", name, dir, size[0] != '\0' ? size : "0", desc);
    if (!sel_aminet_parse_line(line, s->row)) {
        return 1; /* another directory or file type: like a sync's skipped row */
    }
    s->given++;
    return s->sink->row(s->sink->ctx, s->row->path, s->row->name, s->row->artist, SEL_AMINET_FORMAT);
}

int32_t sel_aminet_search_file(const sel_host* host, const char* file, const sel_search_sink* out) {
    search_state s;
    char* page = host->mem_alloc(host, SEARCH_PAGE_MAX + 1u, SEL_MEM_FAST);
    FILE* f;
    size_t got;
    const char* p;
    int32_t n = -1;

    s.sink = out;
    s.given = 0;
    s.row = host->mem_alloc(host, sizeof *s.row, SEL_MEM_FAST);
    if (page != NULL && s.row != NULL && (f = fopen(file, "rb")) != NULL) {
        got = fread(page, 1, SEARCH_PAGE_MAX, f);
        fclose(f);
        page[got] = '\0';
        for (p = page; (p = strstr(p, "class=\"name_col\"")) != NULL;) {
            const char* end = strstr(p, "</tr>");

            if (end == NULL || !search_row(&s, p, end)) {
                break;
            }
            p = end;
        }
        n = s.given;
    }
    if (page != NULL) {
        host->mem_free(host, page);
    }
    if (s.row != NULL) {
        host->mem_free(host, s.row);
    }
    return n;
}
