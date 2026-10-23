/* See modland_sync.h. No float, no allocation after the start (blocks from the host). */
#include "modland_sync.h"

#include "../local.h"
#include "../source.h"
#include "sync_util.h"

#include <stdio.h>
#include <string.h>

#define LINE_MAX_BYTES 1100u
#define SYNC_BATCH 32u
#define PATH_BYTES 1024u
#define SCRATCH_BYTES 512u
#define MODULES "pub/modules/"
#define WEEK_DAYS 7

typedef struct sync_row_text {
    char path[PATH_BYTES];
    char md5[40];
    char scratch[SCRATCH_BYTES];
} sync_row_text;

typedef struct sync_state {
    const sel_local_scan_config* cfg;
    sel_modland_sync_stats* st;
    const sel_source* src;
    sync_row_text* text;
    sel_scan_row* rows;
    uint32_t row_n;
    int failed;
} sync_state;

static int is_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* "<md5><ws><path>" or "<size>\t<path>": sets *md5 (NULL for the size form) and *path. */
static int split_listing_line(char* line, const char** md5, const char** path) {
    size_t i = 0;

    while (is_hex(line[i])) {
        i++;
    }
    if (i == 32u && (line[i] == ' ' || line[i] == '\t')) {
        line[i++] = '\0';
        *md5 = line;
    } else {
        size_t j = 0;
        while (line[j] >= '0' && line[j] <= '9') {
            j++;
        }
        if (j == 0 || line[j] != '\t') {
            return 0;
        }
        i = j + 1u;
        *md5 = NULL;
    }
    while (line[i] == ' ' || line[i] == '\t') {
        i++;
    }
    *path = line + i;
    return line[i] != '\0';
}

/* Writes "pub/modules/" + rel (or rel when it already starts with it). */
static int full_path(const char* rel, char* out, size_t n) {
    int w;

    if (strncmp(rel, MODULES, sizeof MODULES - 1u) == 0) {
        w = snprintf(out, n, "%s", rel);
    } else {
        w = snprintf(out, n, MODULES "%s", rel);
    }
    return w > 0 && (size_t)w < n;
}

static int flush_rows(sync_state* s) {
    return sel_sync_flush_scan(s->cfg, s->rows, &s->row_n, &s->st->batches, &s->failed);
}

/* Queues one row (tags from the source's parse); the caller flushes. */
static int queue_row(sync_state* s, const char* rel, const char* md5, int64_t size) {
    sync_row_text* t = &s->text[s->row_n];
    sel_scan_row* r = &s->rows[s->row_n];
    sel_tag tags[4];
    uint32_t i;

    if (!full_path(rel, t->path, sizeof t->path) || !s->src->parse(t->path, t->scratch, sizeof t->scratch, tags)) {
        return 0;
    }
    memset(r, 0, sizeof *r);
    r->source = "modland";
    r->path = t->path;
    r->format = "";
    r->artist = "";
    r->name = "";
    for (i = 0; tags[i].tag != SEL_TAG_END; i++) {
        if (tags[i].tag == SEL_SRC_FORMAT) {
            r->format = (const char*)tags[i].value;
        } else if (tags[i].tag == SEL_SRC_ARTIST) {
            r->artist = (const char*)tags[i].value;
        } else if (tags[i].tag == SEL_SRC_NAME) {
            r->name = (const char*)tags[i].value;
        }
    }
    if (md5 != NULL) {
        snprintf(t->md5, sizeof t->md5, "%s", md5);
        r->md5 = t->md5;
    } else {
        r->md5 = "";
    }
    r->size = size;
    s->row_n++;
    return 1;
}

static int sync_listing(sync_state* s, const char* file) {
    FILE* f = fopen(file, "rb");
    char line[LINE_MAX_BYTES];

    if (f == NULL) {
        return 0;
    }
    while (!s->failed && fgets(line, sizeof line, f) != NULL) {
        const char* md5;
        const char* path;

        sel_sync_chomp(line);
        if (line[0] == '\0') {
            continue;
        }
        if (!split_listing_line(line, &md5, &path) || !queue_row(s, path, md5, 0)) {
            s->st->skipped_rows++;
            continue;
        }
        s->st->listing_rows++;
        if (s->row_n == SYNC_BATCH && !flush_rows(s)) {
            break;
        }
    }
    fclose(f);
    return !s->failed && flush_rows(s);
}

static int remove_path(sync_state* s, const char* rel) {
    sel_remove_batch b;
    sel_message reply;
    char path[PATH_BYTES];
    const char* list[1];

    if (!full_path(rel, path, sizeof path)) {
        return 0;
    }
    list[0] = path;
    b.source = "modland";
    b.paths = list;
    b.count = 1;
    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(s->cfg->lib, SEL_LIB_REQ_REMOVE, &b, &reply) || reply.args[1].v.i != 0) {
        s->failed = 1;
        return 0;
    }
    return reply.args[0].v.i > 0;
}

static int add_path(sync_state* s, const char* rel) {
    if (!queue_row(s, rel, NULL, 0)) {
        return 0;
    }
    return flush_rows(s);
}

/* Re-keys the row at `from` to `dest` in place (md5, size, stickers, likes stay). A `from` the catalogue
 * never had is simply added at `dest`. */
static int move_row(sync_state* s, const char* from, const char* dest) {
    sel_move mv;
    sel_message reply;
    char fpath[PATH_BYTES];

    if (!full_path(from, fpath, sizeof fpath) || !queue_row(s, dest, NULL, 0)) {
        return 0;
    }
    s->row_n--;
    mv.source = "modland";
    mv.from = fpath;
    mv.to = &s->rows[s->row_n];
    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(s->cfg->lib, SEL_LIB_REQ_MOVE, &mv, &reply) || reply.args[1].v.i != 0) {
        s->failed = 1;
        return 0;
    }
    if (reply.args[0].v.i == 0) {
        s->row_n++;
        if (!flush_rows(s)) {
            return 0;
        }
    }
    s->st->cl_moved++;
    return 1;
}

/* Days since 1970-01-01 of a civil date (proleptic Gregorian, integers only). */
static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) {
    int64_t era, yoe, doy, doe;

    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* changelog_YYYY-MM-DD.txt: day number, or -1. */
static int64_t changelog_day(const char* name) {
    unsigned y, m, d;
    char tail[8];

    if (strncmp(name, SEL_MODLAND_CHANGELOG_PREFIX, sizeof SEL_MODLAND_CHANGELOG_PREFIX - 1u) != 0) {
        return -1;
    }
    if (sscanf(name + sizeof SEL_MODLAND_CHANGELOG_PREFIX - 1u, "%4u-%2u-%2u%7s", &y, &m, &d, tail) != 4 ||
        strcmp(tail, ".txt") != 0) {
        return -1;
    }
    return days_from_civil((int64_t)y, (int64_t)m, (int64_t)d);
}

/* `moved:` names the old path; the `to:` line is a full path when its last component has an extension,
 * else a directory the file name moves into. */
static int apply_move(sync_state* s, const char* from, const char* to) {
    char dest[PATH_BYTES];
    const char* base = strrchr(from, '/');
    const char* tbase = strrchr(to, '/');
    int w;

    base = base != NULL ? base + 1 : from;
    tbase = tbase != NULL ? tbase + 1 : to;
    if (strchr(tbase, '.') != NULL) {
        w = snprintf(dest, sizeof dest, "%s", to);
    } else {
        w = snprintf(dest, sizeof dest, "%s/%s", to, base);
    }
    if (w <= 0 || (size_t)w >= sizeof dest) {
        return 0;
    }
    return move_row(s, from, dest);
}

static int apply_changelog(sync_state* s, const char* file, int in_week) {
    FILE* f = fopen(file, "rb");
    char line[LINE_MAX_BYTES];
    char from[PATH_BYTES];

    if (f == NULL) {
        return 0;
    }
    from[0] = '\0';
    while (!s->failed && fgets(line, sizeof line, f) != NULL) {
        char* p;
        char* key;

        sel_sync_chomp(line);
        key = line;
        while (*key == ' ' || *key == '\t') {
            key++;
        }
        p = strchr(key, ':');
        if (p == NULL) {
            continue;
        }
        *p++ = '\0';
        while (*p == ' ') {
            p++;
        }
        if (strcmp(key, "added") == 0 || strcmp(key, "new") == 0) {
            if (add_path(s, p)) {
                s->st->cl_added++;
                s->st->new_this_week += (uint32_t)in_week;
            }
        } else if (strcmp(key, "deleted") == 0) {
            if (remove_path(s, p)) {
                s->st->cl_removed++;
            }
        } else if (strcmp(key, "updated") == 0) {
            s->st->cl_updated++;
        } else if (strcmp(key, "moved") == 0) {
            snprintf(from, sizeof from, "%s", p);
        } else if (strcmp(key, "to") == 0 && from[0] != '\0') {
            apply_move(s, from, p);
            from[0] = '\0';
        }
    }
    fclose(f);
    s->st->changelogs++;
    return !s->failed;
}

int sel_modland_sync(const sel_local_scan_config* cfg, const char* dir, int64_t now, sel_modland_sync_stats* stats) {
    sync_state s;
    const sel_host* host = cfg->host;
    sel_browse br;
    char path[SEL_BROWSE_PATH_MAX];
    uint32_t i;
    int ok = 0;
    int64_t today = now / 86400;

    memset(&s, 0, sizeof s);
    memset(stats, 0, sizeof *stats);
    s.cfg = cfg;
    s.st = stats;
    sel_source_register_builtin();
    s.src = sel_source_find("modland");
    s.text = host->mem_alloc(host, (size_t)SYNC_BATCH * sizeof *s.text, SEL_MEM_FAST);
    s.rows = host->mem_alloc(host, (size_t)SYNC_BATCH * sizeof *s.rows, SEL_MEM_FAST);
    if (s.src != NULL && s.text != NULL && s.rows != NULL &&
        sel_browse_join(path, sizeof path, dir, SEL_MODLAND_LISTING) && sync_listing(&s, path) &&
        sel_browse_init(&br, host, 512u, 16384u)) {
        ok = 1;
        if (sel_browse_list(&br, dir)) {
            for (i = 0; ok && i < br.count; i++) {
                const char* name = sel_browse_name(&br, i);
                int64_t day = changelog_day(name);

                if (day >= 0 && sel_browse_join(path, sizeof path, dir, name)) {
                    ok = apply_changelog(&s, path, today - day >= 0 && today - day < WEEK_DAYS);
                }
            }
        }
        sel_browse_free(&br, host);
    }
    if (s.text != NULL) {
        host->mem_free(host, s.text);
    }
    if (s.rows != NULL) {
        host->mem_free(host, s.rows);
    }
    return ok && !s.failed;
}
