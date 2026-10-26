/* See sndh_sync.h. */
#include "sndh_sync.h"

#include "../local.h"
#include "../source.h"
#include "sync_util.h"

#include <stdio.h>
#include <string.h>

#define SYNC_BATCH 32u

/* ---- the zip directory ---- */

static uint32_t le16(const unsigned char* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t le32(const unsigned char* p) {
    return le16(p) | le16(p + 2) << 16;
}

int32_t sel_sndh_read_cd(FILE* f, int (*fn)(void* ctx, const sel_sndh_entry* e), void* ctx) {
    unsigned char h[46];
    sel_sndh_entry e;
    int32_t n = 0;

    for (;;) {
        uint32_t nlen, extra;

        if (fread(h, 1, sizeof h, f) != sizeof h || memcmp(h, "PK\x01\x02", 4) != 0) {
            break;
        }
        nlen = le16(h + 28);
        extra = le16(h + 30) + le16(h + 32);
        e.size = le32(h + 24);
        e.utf8 = (le16(h + 8) & 0x800u) != 0;
        e.too_long = nlen >= sizeof e.name;
        if (e.too_long) {
            e.name[0] = '\0';
            if (fseek(f, (long)nlen, SEEK_CUR) != 0) {
                break;
            }
        } else if (fread(e.name, 1, nlen, f) != nlen) {
            break;
        } else {
            e.name[nlen] = '\0';
        }
        if (fseek(f, (long)extra, SEEK_CUR) != 0) {
            break;
        }
        n++;
        if (!fn(ctx, &e)) {
            break;
        }
    }
    return n > 0 ? n : -1;
}

int sel_sndh_find_directory(const unsigned char* tail, size_t n, uint32_t* cd_offset, uint32_t* cd_size) {
    size_t end;

    if (n < 22u) {
        return 0;
    }
    for (end = n - 22u + 1u; end-- > 0;) { /* the last record wins, like bbmp's rfind */
        if (memcmp(tail + end, "PK\x05\x06", 4) == 0) {
            *cd_size = le32(tail + end + 12);
            *cd_offset = le32(tail + end + 16);
            return *cd_size != 0xFFFFFFFFu && *cd_offset != 0xFFFFFFFFu;
        }
    }
    return 0;
}

unsigned sel_sndh_newest_year(const char* page, size_t n) {
    static const char pre[] = "files/sndh";
    static const char post[] = "_lf.zip";
    unsigned best = 0;
    size_t i;

    for (i = 0; i + sizeof pre - 1u + 4u + sizeof post - 1u <= n; i++) {
        const char* p = page + i;
        unsigned y = 0;
        int k;

        if (memcmp(p, pre, sizeof pre - 1u) != 0) {
            continue;
        }
        p += sizeof pre - 1u;
        for (k = 0; k < 4 && p[k] >= '0' && p[k] <= '9'; k++) {
            y = y * 10u + (unsigned)(p[k] - '0');
        }
        if (k == 4 && memcmp(p + 4, post, sizeof post - 1u) == 0 && y > best) {
            best = y;
        }
    }
    return best;
}

/* ---- the catalogue ---- */

typedef struct row_text {
    char path[SEL_SNDH_PATH_MAX];
    char scratch[SEL_SNDH_PATH_MAX + 32u];
} row_text;

typedef struct sync_state {
    const sel_local_scan_config* cfg;
    sel_sndh_sync_stats* st;
    const sel_source* src;
    row_text* text;
    sel_scan_row* rows;
    uint32_t row_n;
    int failed;
} sync_state;

static int safe_path(const char* p) {
    return p[0] != '\0' && p[0] != '/' && strchr(p, '\\') == NULL && strstr(p, "..") == NULL;
}

static int ends_with_sndh(const char* s) {
    size_t n = strlen(s), i;
    static const char ext[] = ".sndh";

    if (n < sizeof ext - 1u) {
        return 0;
    }
    for (i = 0; i < sizeof ext - 1u; i++) {
        char c = s[n - (sizeof ext - 1u) + i];

        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + 32);
        }
        if (c != ext[i]) {
            return 0;
        }
    }
    return 1;
}

static int on_entry(void* ctx, const sel_sndh_entry* e) {
    sync_state* s = ctx;
    const size_t folder = sizeof SEL_SNDH_FOLDER - 1u;
    row_text* t;
    sel_scan_row* r;
    sel_tag tags[4];
    const char* rel;
    size_t i;

    s->st->entries++;
    if (e->too_long) { /* only a .sndh member would have been a row, and its name does not fit */
        return 1;
    }
    if (strncmp(e->name, SEL_SNDH_FOLDER, folder) != 0 || e->name[strlen(e->name) - 1u] == '/' ||
        !ends_with_sndh(e->name)) {
        return 1;
    }
    rel = e->name + folder;
    for (i = 0; !e->utf8 && rel[i] != '\0'; i++) {
        if ((unsigned char)rel[i] >= 0x80u) { /* CP437 names would need a table; the 2026 archive has none */
            s->st->skipped_rows++;
            return 1;
        }
    }
    t = &s->text[s->row_n];
    r = &s->rows[s->row_n];
    if (!safe_path(rel) || !s->src->parse(rel, t->scratch, sizeof t->scratch, tags)) {
        s->st->skipped_rows++;
        return 1;
    }
    memcpy(t->path, rel, strlen(rel) + 1u);
    memset(r, 0, sizeof *r);
    r->source = "sndh";
    r->path = t->path;
    r->format = r->artist = r->name = "";
    for (i = 0; tags[i].tag != SEL_TAG_END; i++) {
        if (tags[i].tag == SEL_SRC_FORMAT) {
            r->format = (const char*)tags[i].value;
        } else if (tags[i].tag == SEL_SRC_ARTIST) {
            r->artist = (const char*)tags[i].value;
        } else if (tags[i].tag == SEL_SRC_NAME) {
            r->name = (const char*)tags[i].value;
        }
    }
    r->album = r->md5 = "";
    r->size = e->size;
    s->row_n++;
    s->st->listing_rows++;
    if (s->row_n == SYNC_BATCH && !sel_sync_flush_scan(s->cfg, s->rows, &s->row_n, &s->st->batches, &s->failed)) {
        return 0;
    }
    return 1;
}

int sel_sndh_sync(const sel_local_scan_config* cfg, const char* dir, sel_sndh_sync_stats* stats) {
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
    s.src = sel_source_find("sndh");
    s.text = host->mem_alloc(host, (size_t)SYNC_BATCH * sizeof *s.text, SEL_MEM_FAST);
    s.rows = host->mem_alloc(host, (size_t)SYNC_BATCH * sizeof *s.rows, SEL_MEM_FAST);
    if (s.text != NULL && s.rows != NULL && s.src != NULL && sel_browse_join(path, sizeof path, dir, SEL_SNDH_CD)) {
        f = fopen(path, "rb");
    }
    if (f != NULL) {
        ok = sel_sndh_read_cd(f, on_entry, &s) >= 0 && !s.failed &&
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
