/* See hvsc_sync.h. No float, no allocation after the start (blocks from the host). */
#include "hvsc_sync.h"

#include "../local.h"
#include "../source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_BYTES 2400u
#define PATH_BYTES 512u
#define SCAN_BATCH 32u
#define LEN_BATCH 32u
#define LEN_TEXT 1800u
#define STIL_BATCH 16u
#define STIL_STICKERS 24u
#define STIL_NAME 24u
#define STIL_POOL 2048u

typedef struct scan_text {
    char path[PATH_BYTES];
    char scratch[256];
} scan_text;

typedef struct len_item {
    char path[PATH_BYTES];
    char md5[40];
    char lengths[LEN_TEXT];
    char count[8];
    sel_sticker st[2];
} len_item;

typedef struct stil_item {
    char path[PATH_BYTES];
    char names[STIL_STICKERS][STIL_NAME];
    char pool[STIL_POOL];
    uint32_t value_at[STIL_STICKERS];
    sel_sticker st[STIL_STICKERS];
    uint32_t pool_used;
    uint32_t n;
} stil_item;

typedef struct sync_state {
    const sel_local_scan_config* cfg;
    sel_hvsc_sync_stats* st;
    const sel_source* src;
    scan_text* text;
    sel_scan_row* rows;
    uint32_t row_n;
    len_item* lens;
    stil_item* stils;
    sel_annotation* notes;
    uint32_t item_n;
    int failed;
} sync_state;

int32_t sel_hvsc_parse_time_ms(const char* t) {
    int64_t min = 0, sec = 0, ms = 0;
    int digits = 0;

    if (t == NULL || t[0] < '0' || t[0] > '9') {
        return -1;
    }
    for (; *t >= '0' && *t <= '9'; t++) {
        min = min * 10 + (*t - '0');
        if (min > 35000) {
            return -1;
        }
    }
    if (*t++ != ':' || t[0] < '0' || t[0] > '9') {
        return -1;
    }
    for (; *t >= '0' && *t <= '9'; t++) {
        sec = sec * 10 + (*t - '0');
        if (sec > 99999) {
            return -1;
        }
    }
    if (*t == '.') {
        t++;
        for (; *t >= '0' && *t <= '9'; t++) {
            if (digits < 3) {
                ms = ms * 10 + (*t - '0');
                digits++;
            }
        }
    }
    for (; digits < 3; digits++) {
        ms *= 10;
    }
    return (int32_t)((min * 60 + sec) * 1000 + ms);
}

static void chomp(char* s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) {
        s[--n] = '\0';
    }
}

/* Strips leading '/' and an optional "C64Music/" root so every path is relative to the HVSC root. */
static const char* rel_path(const char* p) {
    while (*p == '/') {
        p++;
    }
    if (strncmp(p, "C64Music/", 9) == 0) {
        p += 9;
    }
    return p;
}

static int send_request(sync_state* s, uint32_t req, const void* payload, sel_message* reply) {
    memset(reply, 0, sizeof *reply);
    if (!sel_library_request(s->cfg->lib, req, payload, reply) || reply->args[1].v.i != 0) {
        s->failed = 1;
        return 0;
    }
    return 1;
}

static int flush_rows(sync_state* s) {
    sel_scan_batch b;
    sel_message reply;

    if (s->row_n == 0) {
        return 1;
    }
    b.rows = s->rows;
    b.count = s->row_n;
    b.now = s->cfg->now;
    s->row_n = 0;
    if (!send_request(s, SEL_LIB_REQ_SCAN_BATCH, &b, &reply)) {
        return 0;
    }
    s->st->batches++;
    return 1;
}

static int queue_row(sync_state* s, const char* rel) {
    scan_text* t = &s->text[s->row_n];
    sel_scan_row* r = &s->rows[s->row_n];
    sel_tag tags[4];
    uint32_t i;

    if (strlen(rel) >= sizeof t->path) {
        return 0;
    }
    memcpy(t->path, rel, strlen(rel) + 1u);
    if (!s->src->parse(t->path, t->scratch, sizeof t->scratch, tags)) {
        return 0;
    }
    memset(r, 0, sizeof *r);
    r->source = "hvsc";
    r->path = t->path;
    r->format = r->artist = r->name = r->md5 = "";
    for (i = 0; tags[i].tag != SEL_TAG_END; i++) {
        if (tags[i].tag == SEL_SRC_FORMAT) {
            r->format = (const char*)tags[i].value;
        } else if (tags[i].tag == SEL_SRC_ARTIST) {
            r->artist = (const char*)tags[i].value;
        } else if (tags[i].tag == SEL_SRC_NAME) {
            r->name = (const char*)tags[i].value;
        }
    }
    s->row_n++;
    return 1;
}

static int sync_listing(sync_state* s, const char* file) {
    FILE* f = fopen(file, "rb");
    char line[LINE_BYTES];

    if (f == NULL) {
        return 0;
    }
    while (!s->failed && fgets(line, sizeof line, f) != NULL) {
        chomp(line);
        if (line[0] == '\0' || line[0] == ';' || line[0] == '#') {
            continue;
        }
        if (!queue_row(s, rel_path(line))) {
            s->st->skipped_rows++;
            continue;
        }
        s->st->listing_rows++;
        if (s->row_n == SCAN_BATCH && !flush_rows(s)) {
            break;
        }
    }
    fclose(f);
    return !s->failed && flush_rows(s);
}

static int flush_annotations(sync_state* s, uint32_t* matched) {
    sel_annotate_batch b;
    sel_message reply;

    if (s->item_n == 0) {
        return 1;
    }
    b.source = "hvsc";
    b.items = s->notes;
    b.count = s->item_n;
    s->item_n = 0;
    if (!send_request(s, SEL_LIB_REQ_ANNOTATE, &b, &reply)) {
        return 0;
    }
    *matched += (uint32_t)reply.args[0].v.i;
    return 1;
}

/* "<md5>=<t1> <t2> ...": fills the item and its annotation; returns the subtune count (0 when malformed). */
static uint32_t fill_lengths(len_item* it, char* line, sel_annotation* a) {
    char* eq = strchr(line, '=');
    char* p;
    size_t used = 0;
    uint32_t n = 0;

    if (eq == NULL || eq - line != 32) {
        return 0;
    }
    *eq = '\0';
    memcpy(it->md5, line, 33u);
    memset(a, 0, sizeof *a);
    a->path = it->path;
    a->md5 = it->md5;
    it->lengths[0] = '\0';
    for (p = eq + 1; *p != '\0';) {
        int32_t ms;
        int w;

        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        ms = sel_hvsc_parse_time_ms(p);
        if (ms < 0) {
            return 0;
        }
        w = snprintf(it->lengths + used, LEN_TEXT - used, n == 0 ? "%ld" : ",%ld", (long)ms);
        if (w <= 0 || used + (size_t)w >= LEN_TEXT) {
            return 0;
        }
        used += (size_t)w;
        if (n == 0) {
            a->duration_ms = ms;
        }
        n++;
        while (*p != ' ' && *p != '\0') {
            p++;
        }
    }
    if (n == 0) {
        return 0;
    }
    snprintf(it->count, sizeof it->count, "%lu", (unsigned long)n);
    it->st[0].name = "subtunes";
    it->st[0].value = it->count;
    it->st[1].name = "lengths_ms";
    it->st[1].value = it->lengths;
    a->stickers = it->st;
    a->sticker_n = 2;
    return n;
}

static int sync_songlengths(sync_state* s, const char* file) {
    FILE* f = fopen(file, "rb");
    char line[LINE_BYTES];
    char path[PATH_BYTES];
    uint32_t matched = 0;

    if (f == NULL) {
        return 1; /* optional */
    }
    path[0] = '\0';
    s->item_n = 0;
    while (!s->failed && fgets(line, sizeof line, f) != NULL) {
        len_item* it;
        uint32_t n;

        chomp(line);
        if (line[0] == ';') {
            const char* p = line + 1;

            while (*p == ' ') {
                p++;
            }
            if (*p == '/' && strlen(rel_path(p)) < sizeof path) {
                strcpy(path, rel_path(p));
            }
            continue;
        }
        if (line[0] == '[' || line[0] == '\0' || path[0] == '\0') {
            continue;
        }
        it = &s->lens[s->item_n];
        strcpy(it->path, path);
        n = fill_lengths(it, line, &s->notes[s->item_n]);
        path[0] = '\0';
        if (n == 0) {
            continue;
        }
        s->st->songlengths++;
        s->st->subtunes += n;
        s->item_n++;
        if (s->item_n == LEN_BATCH && !flush_annotations(s, &matched)) {
            break;
        }
    }
    fclose(f);
    if (!s->failed) {
        flush_annotations(s, &matched);
    }
    s->st->lengths_matched = matched;
    return !s->failed;
}

static int stil_field(const char* t, const char** name, size_t* skip) {
    static const char* const kw[] = {"COMMENT:", "TITLE:", "ARTIST:", "AUTHOR:", "NAME:"};
    static const char* const lower[] = {"comment", "title", "artist", "author", "name"};
    unsigned i;

    for (i = 0; i < 5u; i++) {
        size_t n = strlen(kw[i]);

        if (strncmp(t, kw[i], n) == 0) {
            *name = lower[i];
            *skip = n;
            return 1;
        }
    }
    return 0;
}

static void stil_begin(stil_item* it, const char* path) {
    it->n = 0;
    it->pool_used = 0;
    snprintf(it->path, sizeof it->path, "%s", path);
}

/* Closes the entry being built in slot item_n; sends the batch when full. */
static void stil_commit(sync_state* s, uint32_t* matched) {
    stil_item* it = &s->stils[s->item_n];
    sel_annotation* a = &s->notes[s->item_n];
    uint32_t i;

    if (it->path[0] == '\0' || it->n == 0) {
        it->path[0] = '\0';
        return;
    }
    for (i = 0; i < it->n; i++) {
        it->st[i].name = it->names[i];
        it->st[i].value = it->pool + it->value_at[i];
    }
    memset(a, 0, sizeof *a);
    a->path = it->path;
    a->stickers = it->st;
    a->sticker_n = it->n;
    s->st->stil_entries++;
    s->item_n++;
    if (s->item_n == STIL_BATCH) {
        flush_annotations(s, matched);
    }
}

static void stil_add(stil_item* it, unsigned subtune, const char* field, const char* value) {
    size_t len = strlen(value);
    int w;

    if (it->n >= STIL_STICKERS || it->pool_used + len + 1u > STIL_POOL) {
        return;
    }
    w = subtune == 0 ? snprintf(it->names[it->n], STIL_NAME, "stil.%s", field)
                     : snprintf(it->names[it->n], STIL_NAME, "stil.%u.%s", subtune, field);
    if (w <= 0 || (size_t)w >= STIL_NAME) {
        return;
    }
    it->value_at[it->n] = it->pool_used;
    memcpy(it->pool + it->pool_used, value, len + 1u);
    it->pool_used += (uint32_t)len + 1u;
    it->n++;
}

/* Continuation line: appended to the last value, which is the last thing in the pool. */
static void stil_append(stil_item* it, const char* text) {
    size_t len = strlen(text);

    if (it->n == 0 || it->pool_used + len + 1u > STIL_POOL) {
        return;
    }
    it->pool[it->pool_used - 1u] = ' ';
    memcpy(it->pool + it->pool_used, text, len + 1u);
    it->pool_used += (uint32_t)len;
}

static int sync_stil(sync_state* s, const char* file) {
    FILE* f = fopen(file, "rb");
    char line[LINE_BYTES];
    uint32_t matched = 0;
    unsigned subtune = 0;
    int open_field = 0;

    if (f == NULL) {
        return 1; /* optional */
    }
    s->item_n = 0;
    s->stils[0].path[0] = '\0';
    while (!s->failed && fgets(line, sizeof line, f) != NULL) {
        stil_item* it;
        const char* t = line;
        const char* field;
        size_t skip;

        chomp(line);
        if (line[0] == '/') {
            stil_commit(s, &matched);
            it = &s->stils[s->item_n];
            stil_begin(it, rel_path(line));
            subtune = 0;
            open_field = 0;
            continue;
        }
        it = &s->stils[s->item_n];
        if (it->path[0] == '\0') {
            continue;
        }
        while (*t == ' ' || *t == '\t') {
            t++;
        }
        if (*t == '\0' || *t == '#') {
            open_field = 0;
        } else if (t[0] == '(' && t[1] == '#') {
            subtune = (unsigned)strtoul(t + 2, NULL, 10);
            open_field = 0;
        } else if (stil_field(t, &field, &skip)) {
            t += skip;
            while (*t == ' ') {
                t++;
            }
            stil_add(it, subtune, field, t);
            open_field = 1;
        } else if (open_field && line[0] == ' ') {
            stil_append(it, t);
        }
    }
    fclose(f);
    if (!s->failed) {
        stil_commit(s, &matched);
        flush_annotations(s, &matched);
    }
    s->st->stil_matched = matched;
    return !s->failed;
}

int sel_hvsc_sync(const sel_local_scan_config* cfg, const char* dir, sel_hvsc_sync_stats* stats) {
    sync_state s;
    const sel_host* host = cfg->host;
    char path[SEL_BROWSE_PATH_MAX];
    int ok = 0;

    memset(&s, 0, sizeof s);
    memset(stats, 0, sizeof *stats);
    s.cfg = cfg;
    s.st = stats;
    sel_source_register_builtin();
    s.src = sel_source_find("hvsc");
    s.text = host->mem_alloc(host, (size_t)SCAN_BATCH * sizeof *s.text, SEL_MEM_FAST);
    s.rows = host->mem_alloc(host, (size_t)SCAN_BATCH * sizeof *s.rows, SEL_MEM_FAST);
    s.lens = host->mem_alloc(host, (size_t)LEN_BATCH * sizeof *s.lens, SEL_MEM_FAST);
    s.stils = host->mem_alloc(host, (size_t)(STIL_BATCH + 1u) * sizeof *s.stils, SEL_MEM_FAST);
    s.notes = host->mem_alloc(host, (size_t)LEN_BATCH * sizeof *s.notes, SEL_MEM_FAST);
    if (s.src != NULL && s.text != NULL && s.rows != NULL && s.lens != NULL && s.stils != NULL && s.notes != NULL &&
        sel_browse_join(path, sizeof path, dir, SEL_HVSC_LISTING) && sync_listing(&s, path)) {
        ok = sel_browse_join(path, sizeof path, dir, SEL_HVSC_SONGLENGTHS) && sync_songlengths(&s, path);
        if (ok) {
            ok = sel_browse_join(path, sizeof path, dir, SEL_HVSC_STIL) && sync_stil(&s, path);
        }
    }
    if (s.text != NULL) {
        host->mem_free(host, s.text);
    }
    if (s.rows != NULL) {
        host->mem_free(host, s.rows);
    }
    if (s.lens != NULL) {
        host->mem_free(host, s.lens);
    }
    if (s.stils != NULL) {
        host->mem_free(host, s.stils);
    }
    if (s.notes != NULL) {
        host->mem_free(host, s.notes);
    }
    return ok && !s.failed;
}
