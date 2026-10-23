/* See hippo_sync.h. No float, no allocation after the start (blocks from the host). */
#include "hippo_sync.h"

#include "../local.h"
#include "../source.h"
#include "hippo_prg.h"
#include "sync_util.h"

#include <stdio.h>
#include <string.h>

#define LINE_BYTES 1100u
#define SYNC_BATCH 32u
#define F_FILE 256u
#define F_TITLE 256u
#define F_AUTHOR 128u
#define F_GENRE 64u
#define F_ID 16u
#define FORMAT_NAME "Hippo - Playlists"
#define PRG_MAX_BYTES (4u * 1024u * 1024u)

typedef struct list_item {
    char file[F_FILE];
    char title[F_TITLE];
    char author[F_AUTHOR];
    char genre[F_GENRE];
    char id[F_ID];
    sel_sticker st[2];
} list_item;

typedef struct sync_state {
    const sel_local_scan_config* cfg;
    sel_hippo_sync_stats* st;
    list_item* items; /* SYNC_BATCH slots; slot n is the one being filled */
    sel_scan_row* rows;
    sel_annotation* notes;
    uint32_t n;
    int failed;
} sync_state;

static int send_request(sync_state* s, uint32_t req, const void* payload) {
    sel_message reply;

    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(s->cfg->lib, req, payload, &reply) || reply.args[1].v.i != 0) {
        s->failed = 1;
        return 0;
    }
    return 1;
}

/* Writes the queued rows, then their stickers (an annotation needs the song row to exist). */
static int flush_items(sync_state* s) {
    sel_scan_batch b;
    sel_annotate_batch a;

    if (s->n == 0) {
        return 1;
    }
    b.rows = s->rows;
    b.count = s->n;
    b.now = s->cfg->now;
    a.source = "hippo";
    a.items = s->notes;
    a.count = s->n;
    s->n = 0;
    if (!send_request(s, SEL_LIB_REQ_SCAN_BATCH, &b)) {
        return 0;
    }
    s->st->batches++;
    return send_request(s, SEL_LIB_REQ_ANNOTATE, &a);
}

/* Slot s->n holds a parsed record: counts it and queues its row and stickers. */
static int commit_item(sync_state* s) {
    list_item* it = &s->items[s->n];
    sel_scan_row* r = &s->rows[s->n];
    sel_annotation* a = &s->notes[s->n];
    uint32_t k = 0;

    if (it->file[0] == '\0' || strchr(it->file, '/') != NULL) {
        s->st->skipped++;
        return 1;
    }
    memset(r, 0, sizeof *r);
    r->source = "hippo";
    r->path = it->file;
    r->format = FORMAT_NAME;
    r->artist = it->author;
    r->name = it->title;
    r->md5 = "";
    memset(a, 0, sizeof *a);
    a->path = it->file;
    if (it->genre[0] != '\0') {
        it->st[k].name = "genre";
        it->st[k++].value = it->genre;
    }
    if (it->id[0] != '\0') {
        it->st[k].name = "hippo_id";
        it->st[k++].value = it->id;
    }
    a->stickers = it->st;
    a->sticker_n = k;
    s->st->lists++;
    s->n++;
    return s->n < SYNC_BATCH || flush_items(s);
}

static void clear_item(list_item* it) {
    it->file[0] = it->title[0] = it->author[0] = it->genre[0] = it->id[0] = '\0';
}

/* Latin-1 bytes to UTF-8 (the catalogue stores UTF-8); truncates at n - 1. */
static void latin1_to_utf8(char* dst, size_t n, const char* src, size_t len) {
    size_t o = 0, i;

    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)src[i];

        if (c < 0x80u) {
            if (o + 1u >= n) {
                break;
            }
            dst[o++] = (char)c;
        } else {
            if (o + 2u >= n) {
                break;
            }
            dst[o++] = (char)(0xC0u | (c >> 6));
            dst[o++] = (char)(0x80u | (c & 0x3Fu));
        }
    }
    dst[o] = '\0';
}

/* Legacy feed: `Filename\tPath\tTitle\tAuthor\tGenre` per line, Latin-1. The Path column is the old
 * server path and unused. `#` lines are comments (fixture provenance). */
static int sync_tsv(sync_state* s, const char* file) {
    FILE* f = fopen(file, "rb");
    char line[LINE_BYTES];

    if (f == NULL) {
        return 0;
    }
    while (!s->failed && fgets(line, sizeof line, f) != NULL) {
        const char* col[5];
        size_t len[5];
        char* p = line;
        list_item* it = &s->items[s->n];
        unsigned c;

        sel_sync_chomp(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        for (c = 0; c < 5u; c++) {
            char* tab = strchr(p, '\t');

            col[c] = p;
            len[c] = tab != NULL ? (size_t)(tab - p) : strlen(p);
            p = tab != NULL ? tab + 1 : p + len[c];
        }
        if (len[0] == 8u && memcmp(col[0], "Filename", 8) == 0) {
            continue;
        }
        clear_item(it);
        latin1_to_utf8(it->file, sizeof it->file, col[0], len[0]);
        latin1_to_utf8(it->title, sizeof it->title, col[2], len[2]);
        latin1_to_utf8(it->author, sizeof it->author, col[3], len[3]);
        latin1_to_utf8(it->genre, sizeof it->genre, col[4], len[4]);
        if (!commit_item(s)) {
            break;
        }
    }
    fclose(f);
    return !s->failed && flush_items(s);
}

static int hex4(FILE* f) {
    int v = 0, i;

    for (i = 0; i < 4; i++) {
        int c = getc(f);

        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'a' && c <= 'f') {
            c = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            c = c - 'A' + 10;
        } else {
            return -1;
        }
        v = v * 16 + c;
    }
    return v;
}

/* After the opening quote: reads to the closing quote, decoding JSON escapes to UTF-8; truncates at n - 1. */
static void json_string(FILE* f, char* buf, size_t n) {
    size_t o = 0;
    int c;

    while ((c = getc(f)) != EOF && c != '"') {
        char enc[4];
        size_t k, ne = 1;

        if (c == '\\') {
            c = getc(f);
            if (c == 'u') {
                int u = hex4(f);

                if (u < 0) {
                    break;
                }
                if (u < 0x80) {
                    enc[0] = (char)u;
                } else if (u < 0x800) {
                    enc[0] = (char)(0xC0 | (u >> 6));
                    enc[1] = (char)(0x80 | (u & 0x3F));
                    ne = 2;
                } else {
                    enc[0] = (char)(0xE0 | (u >> 12));
                    enc[1] = (char)(0x80 | ((u >> 6) & 0x3F));
                    enc[2] = (char)(0x80 | (u & 0x3F));
                    ne = 3;
                }
            } else {
                enc[0] = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : (char)c;
            }
        } else {
            enc[0] = (char)c;
        }
        if (o + ne < n) {
            for (k = 0; k < ne; k++) {
                buf[o++] = enc[k];
            }
        }
    }
    if (n > 0) {
        buf[o] = '\0';
    }
}

/* One object of the list: flat "key": "string" | number members. Unknown keys are read and dropped. */
static int json_object(sync_state* s, FILE* f) {
    list_item* it = &s->items[s->n];
    char key[24];
    char scratch[F_TITLE];
    int c;

    clear_item(it);
    while ((c = getc(f)) != EOF && c != '}') {
        char* dst = scratch;
        size_t dn = sizeof scratch;
        size_t o = 0;

        if (c != '"') {
            continue;
        }
        json_string(f, key, sizeof key);
        while ((c = getc(f)) != EOF && (c == ' ' || c == ':' || c == '\n' || c == '\r' || c == '\t')) {
        }
        if (strcmp(key, "filename") == 0) {
            dst = it->file, dn = sizeof it->file;
        } else if (strcmp(key, "title") == 0) {
            dst = it->title, dn = sizeof it->title;
        } else if (strcmp(key, "author") == 0) {
            dst = it->author, dn = sizeof it->author;
        } else if (strcmp(key, "genre") == 0) {
            dst = it->genre, dn = sizeof it->genre;
        } else if (strcmp(key, "id") == 0) {
            dst = it->id, dn = sizeof it->id;
        }
        if (c == '"') {
            json_string(f, dst, dn);
        } else {
            while (c != EOF && c != ',' && c != '}' && c != ' ' && c != '\n' && c != '\r') {
                if (o + 1u < dn) {
                    dst[o++] = (char)c;
                }
                c = getc(f);
            }
            dst[o] = '\0';
            if (c == '}') {
                break;
            }
        }
    }
    return commit_item(s);
}

static int sync_json(sync_state* s, const char* file) {
    FILE* f = fopen(file, "rb");
    int c;

    if (f == NULL) {
        return 0;
    }
    while (!s->failed && (c = getc(f)) != EOF) {
        if (c == '{' && !json_object(s, f)) {
            break;
        }
    }
    fclose(f);
    return !s->failed && flush_items(s);
}

static int ends_with_prg(const char* name) {
    size_t n = strlen(name), e = sizeof SEL_HIPPO_PRG_EXT - 1u;

    return n > e && strcmp(name + n - e, SEL_HIPPO_PRG_EXT) == 0;
}

/* A fetched .prg: the Hippo codec reads it; the counts become stickers on the listed playlist. */
static int apply_prg(sync_state* s, const char* name, const char* file) {
    const sel_host* host = s->cfg->host;
    FILE* f = fopen(file, "rb");
    long size;
    char* bytes = NULL;
    sel_hippo_prg_stats ps;
    int ok = 1;

    if (f == NULL) {
        return 1;
    }
    if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) > 0 && (unsigned long)size <= PRG_MAX_BYTES &&
        fseek(f, 0, SEEK_SET) == 0) {
        bytes = host->mem_alloc(host, (size_t)size, SEL_MEM_FAST);
        if (bytes != NULL && fread(bytes, 1, (size_t)size, f) == (size_t)size &&
            sel_hippo_prg_scan(host, bytes, (size_t)size, &ps)) {
            char num[4][12];
            sel_sticker st[4];
            sel_annotation a;
            sel_annotate_batch b;

            snprintf(num[0], sizeof num[0], "%lu", (unsigned long)ps.entries);
            snprintf(num[1], sizeof num[1], "%lu", (unsigned long)ps.dividers);
            snprintf(num[2], sizeof num[2], "%lu", (unsigned long)ps.stealth);
            snprintf(num[3], sizeof num[3], "%lu", (unsigned long)ps.subsongs);
            st[0].name = "prg_entries";
            st[1].name = "prg_dividers";
            st[2].name = "prg_stealth";
            st[3].name = "prg_subsongs";
            st[0].value = num[0];
            st[1].value = num[1];
            st[2].value = num[2];
            st[3].value = num[3];
            memset(&a, 0, sizeof a);
            a.path = name;
            a.stickers = st;
            a.sticker_n = 4;
            b.source = "hippo";
            b.items = &a;
            b.count = 1;
            s->st->prg_files++;
            s->st->prg_entries += ps.entries;
            s->st->prg_dividers += ps.dividers;
            s->st->prg_stealth += ps.stealth;
            s->st->prg_subsongs += ps.subsongs;
            ok = send_request(s, SEL_LIB_REQ_ANNOTATE, &b);
        }
        if (bytes != NULL) {
            host->mem_free(host, bytes);
        }
    }
    fclose(f);
    return ok;
}

int sel_hippo_sync(const sel_local_scan_config* cfg, const char* dir, sel_hippo_sync_stats* stats) {
    sync_state s;
    const sel_host* host = cfg->host;
    char path[SEL_BROWSE_PATH_MAX];
    sel_browse br;
    int have_list = 0;
    int ok = 0;

    memset(&s, 0, sizeof s);
    memset(stats, 0, sizeof *stats);
    s.cfg = cfg;
    s.st = stats;
    sel_source_register_builtin();
    s.items = host->mem_alloc(host, (size_t)SYNC_BATCH * sizeof *s.items, SEL_MEM_FAST);
    s.rows = host->mem_alloc(host, (size_t)SYNC_BATCH * sizeof *s.rows, SEL_MEM_FAST);
    s.notes = host->mem_alloc(host, (size_t)SYNC_BATCH * sizeof *s.notes, SEL_MEM_FAST);
    if (sel_source_find("hippo") != NULL && s.items != NULL && s.rows != NULL && s.notes != NULL) {
        if (sel_browse_join(path, sizeof path, dir, SEL_HIPPO_LIST_JSON) && sync_json(&s, path)) {
            have_list = 1;
        }
        if (!s.failed && sel_browse_join(path, sizeof path, dir, SEL_HIPPO_LIST_TSV) && sync_tsv(&s, path)) {
            have_list = 1;
        }
        ok = have_list && !s.failed;
        if (ok && sel_browse_init(&br, host, 512u, 16384u)) {
            if (sel_browse_list(&br, dir)) {
                uint32_t i;

                for (i = 0; ok && i < br.count; i++) {
                    const char* name = sel_browse_name(&br, i);

                    if (ends_with_prg(name) && sel_browse_join(path, sizeof path, dir, name)) {
                        ok = apply_prg(&s, name, path);
                    }
                }
            }
            sel_browse_free(&br, host);
        }
    }
    if (s.items != NULL) {
        host->mem_free(host, s.items);
    }
    if (s.rows != NULL) {
        host->mem_free(host, s.rows);
    }
    if (s.notes != NULL) {
        host->mem_free(host, s.notes);
    }
    return ok && !s.failed;
}
