/* See aminet_sync.h. No float, one block from the host. */
#include "aminet_sync.h"

#include "source/sync_text.h"

#include <selecta/catalogue.h>

#include <stdio.h>
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
    size_t name_len = 0, dir_len = 0, size_len = 0, age_len = 0, ext, stem_len, n;
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
    /* The artist: the text after the last " by " of the description, up to two spaces (the rating column follows). */
    for (; (p = strstr(p, " by ")) != NULL; p += 4) {
        by = p + 4;
    }
    n = 0;
    if (by != NULL) {
        while (by[n] != '\0' && !(by[n] == ' ' && (by[n + 1u] == ' ' || by[n + 1u] == '\0'))) {
            n++;
        }
    }
    if (n == 0 || n >= sizeof row->artist) {
        memcpy(row->artist, UNKNOWN_ARTIST, sizeof UNKNOWN_ARTIST);
    } else {
        memcpy(row->artist, by, n);
        row->artist[n] = '\0';
    }
    return 1;
}

typedef struct sync_state {
    const sel_host* host;
    const sel_catalogue* cat;
    sel_aminet_sync_stats* st;
    sel_aminet_row* row;
    int failed;
} sync_state;

static int on_line(sync_state* s, const char* line) {
    sel_catalogue_song song;

    if (line[0] == '|' || line[0] == '\0') {
        return 1;
    }
    s->st->lines++;
    s->st->parse_calls++;
    if (!sel_aminet_parse_line(line, s->row)) {
        s->st->skipped_rows++;
        return 1;
    }
    memset(&song, 0, sizeof song);
    song.path = s->row->path;
    song.format = SEL_AMINET_FORMAT;
    song.artist = s->row->artist;
    song.album = song.md5 = "";
    song.name = s->row->name;
    song.size = s->row->size;
    s->st->listing_rows++;
    if (!s->cat->song(s->host, &song)) {
        s->failed = 1;
        return 0;
    }
    return 1;
}

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
            ok = read_index(f, &s) && !s.failed;
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
