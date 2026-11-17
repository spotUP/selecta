/* Host baker for the audacious-uade-tools derived files (I774k, design rows R1-R3); formats in songlen.h and
 * uadedb.h. One entry table (the sorted keys of songlengths.bin) is shared by every output. Not part of
 * selecta_library. The md5 to xxh32 map is a port of upstream's md5ToXxh32 (songdb/scripts/xxh32.sc): md5 first 12 hex
 * digits -> xxh32 of the first 256 KB (8 hex) + file size & 0xFFFF (4 hex), from the first row of every source file. */
#include "songlen.h"
#include "uadedb.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Upstream's xxh32DupWhiteList: xxh32 keys where two md5s collide; their twin rows may legitimately differ. */
static const char* const k_whitelist[] = {
    "658fccfa111a", "799845b9d94e", "326bf0e1bcc3", "52c3a0b7c6a6", "94ba90070307", "00a65024e38c", "a52b32727158",
    "9b5752980f4e", "330b37fdbc1d", "f043f90d4bc0", "728d535a6168", "5e151636c8ae", "1b0bf529bc3c", "7f45b20c2063",
    "3a44d245a550", "4037ef7b0123", "d6b5a65088fd", "efd081d569de", "f7e3ca7950b1", "1b0bf529bc45", "95d9578394b5",
    "3de0b3f43c57", "3c9060b7949f", "0ad487eec7af", "cf7ea0252332", "06d624e80137", "dc221903fae0", "0ea3a79c9cc2",
    "e4c678efeaf2"};

#define LINE_MAX_BYTES 65536

static void put16(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}
static void put24(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 16);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)v;
}
static void put32(uint8_t* p, uint32_t v) {
    put16(p, v >> 16);
    put16(p + 2, v & 0xffffu);
}
static uint32_t get32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* Splits a tab-separated line in place; returns the field count (up to max). */
static int split_tabs(char* line, char** f, int max) {
    int n = 0;
    for (;;) {
        f[n++] = line;
        line = strchr(line, '\t');
        if (line == NULL || n == max) return n;
        *line++ = '\0';
    }
}

/* Calls cb for each line of `path` split into fields. Overlong lines are skipped. Returns 0 if the file is missing. */
typedef void (*row_cb)(char** f, int nf, void* ctx);
static int each_row(const char* path, row_cb cb, void* ctx) {
    FILE* in = fopen(path, "rb");
    char* line = (char*)malloc(LINE_MAX_BYTES);
    char* f[16];
    if (in == NULL || line == NULL) {
        if (in != NULL) fclose(in);
        free(line);
        return 0;
    }
    while (fgets(line, LINE_MAX_BYTES, in) != NULL) {
        size_t n = strlen(line);
        if (n > 0 && line[n - 1] != '\n') { /* overlong line: drop its rest */
            int c;
            while ((c = fgetc(in)) != EOF && c != '\n') {}
            continue;
        }
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        cb(f, split_tabs(line, f, 16), ctx);
    }
    fclose(in);
    free(line);
    return 1;
}

/* ---- string pool: interned NUL-terminated strings, ref 0 = "" ---- */
typedef struct pool {
    char* buf;
    size_t len, cap;
    uint32_t* tab; /* open addressing; stores ref + 1, 0 = empty slot */
    size_t tcap, used;
} pool;

static uint32_t hash_str(const char* s) {
    uint32_t h = 2166136261u;
    while (*s != '\0') h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}

/* Ref of `s`, or -1 if absent; with add, interns it (-2 on overflow/out of memory). */
static long pool_ref(pool* p, const char* s, int add) {
    size_t i, n = strlen(s) + 1;
    if (p->tab == NULL) {
        p->tcap = 1u << 16;
        p->tab = (uint32_t*)calloc(p->tcap, sizeof *p->tab);
        p->cap = 1u << 20;
        p->buf = (char*)malloc(p->cap);
        if (p->tab == NULL || p->buf == NULL) return -2;
        p->buf[0] = '\0';
        p->len = 1;
    }
    if (s[0] == '\0') return 0;
    for (i = hash_str(s) & (p->tcap - 1); p->tab[i] != 0; i = (i + 1) & (p->tcap - 1))
        if (strcmp(p->buf + p->tab[i] - 1, s) == 0) return (long)(p->tab[i] - 1);
    if (!add) return -1;
    if (p->len + n > 0xffffffu) return -2; /* u24 refs */
    if (p->len + n > p->cap) {
        char* g = (char*)realloc(p->buf, p->cap * 2);
        if (g == NULL) return -2;
        p->buf = g;
        p->cap *= 2;
    }
    if ((p->used + 1) * 2 > p->tcap) { /* grow and rehash */
        size_t oc = p->tcap, j;
        uint32_t* old = p->tab;
        p->tcap = oc * 2;
        p->tab = (uint32_t*)calloc(p->tcap, sizeof *p->tab);
        if (p->tab == NULL) return -2;
        for (j = 0; j < oc; j++) {
            if (old[j] != 0) {
                size_t k = hash_str(p->buf + old[j] - 1) & (p->tcap - 1);
                while (p->tab[k] != 0) k = (k + 1) & (p->tcap - 1);
                p->tab[k] = old[j];
            }
        }
        free(old);
        for (i = hash_str(s) & (p->tcap - 1); p->tab[i] != 0; i = (i + 1) & (p->tcap - 1)) {}
    }
    memcpy(p->buf + p->len, s, n);
    p->tab[i] = (uint32_t)p->len + 1u;
    p->len += n;
    p->used++;
    return (long)(p->len - n);
}

/* ---- baking state ---- */
typedef struct info_row {
    uint16_t fmt;
    uint8_t chan, year;
    uint32_t authors, publishers, album;
} info_row;

typedef struct md5_row {
    uint8_t md5[6], key[6];
    uint32_t seq;
} md5_row;

typedef struct bake {
    uint8_t* keys; /* 6 bytes per entry, sorted (songlengths.bin) */
    uint32_t n;
    info_row* info;
    pool pl;
    uint32_t* fmt_ref; /* format id -> string ref */
    uint32_t nfmt;
    md5_row* m;
    size_t nm, capm;
    sel_uadedb_stats* st;
    int failed;
    int pass; /* 0 = fill info from the xxh32 rows, 1 = cross-check the md5 twin rows */
    int is_meta;
} bake;

static long find_entry(const bake* b, const uint8_t key[6]) {
    uint32_t lo = 0, hi = b->n;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        int c = memcmp(key, b->keys + (size_t)mid * 6u, 6);
        if (c == 0) return (long)mid;
        if (c < 0) hi = mid;
        else lo = mid + 1u;
    }
    return -1;
}

static int load_keys(bake* b, const char* songlen_bin) {
    FILE* f = fopen(songlen_bin, "rb");
    uint8_t h[SEL_SONGLEN_HEADER], e[SEL_SONGLEN_ENTRY];
    uint32_t i;
    if (f == NULL) return 0;
    if (fread(h, 1, sizeof h, f) != sizeof h) {
        fclose(f);
        return 0;
    }
    b->n = get32(h + 8);
    b->keys = (uint8_t*)malloc((size_t)b->n * 6u + 1u);
    b->info = (info_row*)calloc((size_t)b->n + 1u, sizeof *b->info);
    for (i = 0; b->keys != NULL && b->info != NULL && i < b->n; i++) {
        if (fread(e, 1, sizeof e, f) != sizeof e) break;
        memcpy(b->keys + (size_t)i * 6u, e, 6);
    }
    fclose(f);
    return b->keys != NULL && b->info != NULL && i == b->n;
}

static int fmt_id(bake* b, long ref) {
    uint32_t i;
    for (i = 0; i < b->nfmt; i++)
        if (b->fmt_ref[i] == (uint32_t)ref) return (int)i;
    if (b->nfmt >= 0xffffu) return -1;
    b->fmt_ref = (uint32_t*)realloc(b->fmt_ref, (b->nfmt + 1u) * sizeof *b->fmt_ref);
    if (b->fmt_ref == NULL) return -1;
    b->fmt_ref[b->nfmt] = (uint32_t)ref;
    return (int)b->nfmt++;
}

static int whitelisted(const uint8_t key[6]) {
    uint8_t w[6];
    size_t i;
    for (i = 0; i < sizeof k_whitelist / sizeof *k_whitelist; i++)
        if (sel_songlen_key_bytes(k_whitelist[i], w) && memcmp(w, key, 6) == 0) return 1;
    return 0;
}

static unsigned year_byte(const char* s) {
    unsigned long y = strtoul(s, NULL, 10);
    return y >= 1901ul && y <= 2155ul ? (unsigned)(y - 1900ul) : 0u;
}

/* One modinfos (key, format, channels) or metadata (key, authors, publishers, album, year) row, from either the
 * xxh32 file (pass 0: fills the entry) or the md5 twin (pass 1: the row, re-keyed, must equal the entry). */
static void info_cb(char** f, int nf, void* ctx) {
    bake* b = (bake*)ctx;
    uint8_t key[6];
    long e, r1, r2, r3, rf;
    info_row row, *cur;
    int fid;
    if (b->failed || nf < 2 || strlen(f[0]) != 12 || !sel_songlen_key_bytes(f[0], key)) return;
    if (b->pass == 1) { /* md5 twin: key column is the md5 prefix; map to the xxh32 key first */
        size_t lo = 0, hi = b->nm;
        int found = 0;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2u;
            int c = memcmp(key, b->m[mid].md5, 6);
            if (c == 0) {
                memcpy(key, b->m[mid].key, 6);
                found = 1;
                break;
            }
            if (c < 0) hi = mid;
            else lo = mid + 1u;
        }
        if (!found) {
            b->st->md5_twin_unmapped++;
            return;
        }
    }
    e = find_entry(b, key);
    if (e < 0) {
        if (b->pass == 0) b->st->info_unmapped++;
        else b->st->md5_twin_unmapped++;
        return;
    }
    cur = &b->info[e];
    row = *cur;
    if (!b->is_meta) {
        int ch = nf > 2 ? atoi(f[2]) : 0;
        rf = pool_ref(&b->pl, f[1], b->pass == 0);
        if (rf < 0 && rf != -1) { b->failed = 1; return; }
        fid = rf >= 0 ? fmt_id(b, rf) : -1;
        if (b->pass == 0 && fid < 0) { b->failed = 1; return; }
        row.fmt = (uint16_t)(fid < 0 ? 0xffffu : fid);
        row.chan = (uint8_t)(ch > 0 && ch < 256 ? ch : 0);
    } else {
        r1 = pool_ref(&b->pl, nf > 1 ? f[1] : "", b->pass == 0);
        r2 = pool_ref(&b->pl, nf > 2 ? f[2] : "", b->pass == 0);
        r3 = pool_ref(&b->pl, nf > 3 ? f[3] : "", b->pass == 0);
        if ((r1 < 0 && r1 != -1) || (r2 < 0 && r2 != -1) || (r3 < 0 && r3 != -1)) { b->failed = 1; return; }
        row.authors = (uint32_t)(r1 < 0 ? 0xffffffu : r1);
        row.publishers = (uint32_t)(r2 < 0 ? 0xffffffu : r2);
        row.album = (uint32_t)(r3 < 0 ? 0xffffffu : r3);
        row.year = (uint8_t)(nf > 4 ? year_byte(f[4]) : 0u);
    }
    if (b->pass == 0) {
        *cur = row;
    } else {
        int same = b->is_meta ? (row.authors == cur->authors && row.publishers == cur->publishers &&
                                 row.album == cur->album && row.year == cur->year)
                              : (row.fmt == cur->fmt && row.chan == cur->chan);
        b->st->md5_checked++;
        if (!same) {
            if (whitelisted(key)) b->st->md5_whitelisted++;
            else b->st->md5_mismatch++;
        }
    }
}

/* Source row: md5, subsong, ms, songend, player, format, channels, size, xxh32, crc32, path. */
static void source_cb(char** f, int nf, void* ctx) {
    bake* b = (bake*)ctx;
    md5_row* r;
    uint8_t md5[6];
    char key[13];
    if (b->failed || nf < 9 || strlen(f[0]) != 32 || strlen(f[8]) != 8 || !sel_songlen_key_bytes(f[0], md5)) return;
    if (b->nm == b->capm) {
        md5_row* g;
        b->capm = b->capm != 0 ? b->capm * 2u : 1u << 16;
        g = (md5_row*)realloc(b->m, b->capm * sizeof *b->m);
        if (g == NULL) { b->failed = 1; return; }
        b->m = g;
    }
    r = &b->m[b->nm];
    snprintf(key, sizeof key, "%s%04lx", f[8], strtoul(f[7], NULL, 10) & 0xffffUL);
    if (!sel_songlen_key_bytes(key, r->key)) return;
    memcpy(r->md5, md5, 6);
    r->seq = (uint32_t)b->nm++;
}

static int cmp_md5(const void* a, const void* b) {
    const md5_row* x = (const md5_row*)a;
    const md5_row* y = (const md5_row*)b;
    int c = memcmp(x->md5, y->md5, 6);
    return c != 0 ? c : (x->seq < y->seq ? -1 : x->seq > y->seq);
}

static void each_source_dir(bake* b, const char* checkout, const char* sub) {
    char dir[1024], path[1300];
    DIR* d;
    struct dirent* de;
    snprintf(dir, sizeof dir, "%s/songdb/sources/%s", checkout, sub);
    d = opendir(dir);
    if (d == NULL) return;
    while ((de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n > 4 && strcmp(de->d_name + n - 4, ".tsv") == 0) {
            snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
            each_row(path, source_cb, b);
        }
    }
    closedir(d);
}

static int write_info(bake* b, const char* out_path, sel_uadedb_stats* st) {
    FILE* f = fopen(out_path, "wb");
    uint8_t h[16], row[SEL_UADEINFO_ROW];
    uint32_t i, pool_at = 16u + b->n * SEL_UADEINFO_ROW;
    int ok = f != NULL;
    if (!ok) return 0;
    memcpy(h, "UINF", 4);
    put32(h + 4, 1u);
    put32(h + 8, b->n);
    put32(h + 12, pool_at);
    ok = fwrite(h, 1, 16, f) == 16;
    for (i = 0; ok && i < b->n; i++) {
        const info_row* r = &b->info[i];
        put16(row, r->fmt);
        row[2] = r->chan;
        put24(row + 3, r->authors);
        put24(row + 6, r->publishers);
        put24(row + 9, r->album);
        row[12] = r->year;
        ok = fwrite(row, 1, sizeof row, f) == sizeof row;
    }
    put32(h, b->nfmt);
    ok = ok && fwrite(h, 1, 4, f) == 4;
    for (i = 0; ok && i < b->nfmt; i++) {
        put32(h, b->fmt_ref[i]);
        ok = fwrite(h, 1, 4, f) == 4;
    }
    ok = ok && fwrite(b->pl.buf, 1, b->pl.len, f) == b->pl.len;
    ok = fclose(f) == 0 && ok;
    st->pool_bytes = (long)b->pl.len;
    st->info_bytes = (long)(pool_at + 4u + 4u * b->nfmt + b->pl.len);
    return ok;
}

static int write_md5(bake* b, const char* out_path, sel_uadedb_stats* st) {
    FILE* f = fopen(out_path, "wb");
    uint8_t h[16] = {0}, row[SEL_UADEMD5_ROW];
    size_t i, w = 0;
    int ok = f != NULL;
    if (!ok) return 0;
    /* b->m is sorted and de-duplicated by md5; keep the rows whose key is an entry */
    for (i = 0; i < b->nm; i++) {
        long e = find_entry(b, b->m[i].key);
        if (e < 0) st->md5_unmapped++;
        else {
            st->md5_mapped++;
            memcpy(b->m[w].md5, b->m[i].md5, 6);
            b->m[w].seq = (uint32_t)e; /* seq now holds the entry */
            w++;
        }
    }
    memcpy(h, "UMD5", 4);
    put32(h + 4, 1u);
    put32(h + 8, (uint32_t)w);
    ok = fwrite(h, 1, 16, f) == 16;
    for (i = 0; ok && i < w; i++) {
        memcpy(row, b->m[i].md5, 6);
        put24(row + 6, b->m[i].seq);
        ok = fwrite(row, 1, sizeof row, f) == sizeof row;
    }
    ok = fclose(f) == 0 && ok;
    st->md5_bytes = (long)(16u + w * SEL_UADEMD5_ROW);
    return ok;
}

int sel_uadedb_bake(const char* checkout, const char* outdir, sel_uadedb_stats* st) {
    static const char* const subs[] = {"site", "collection", "group", "bbs", "cd"};
    char p[1400], q[1400];
    bake b;
    size_t i, w = 0;
    int ok = 0;
    memset(&b, 0, sizeof b);
    memset(st, 0, sizeof *st);
    b.st = st;

    snprintf(p, sizeof p, "%s/tsv/pretty/xxh32/songlengths.tsv", checkout);
    snprintf(q, sizeof q, "%s/songlengths.bin", outdir);
    st->songlen_entries = sel_songlen_bake(p, q, &st->songlen_bytes);
    if (st->songlen_entries < 0 || !load_keys(&b, q)) goto done;

    /* md5 -> xxh32 key map from the source rows (R3); first row in file order wins */
    for (i = 0; i < sizeof subs / sizeof *subs; i++) each_source_dir(&b, checkout, subs[i]);
    if (b.failed) goto done;
    qsort(b.m, b.nm, sizeof *b.m, cmp_md5);
    for (i = 0; i < b.nm; i++) {
        if (w > 0 && memcmp(b.m[w - 1].md5, b.m[i].md5, 6) == 0) {
            if (memcmp(b.m[w - 1].key, b.m[i].key, 6) != 0) st->md5_conflict++;
            continue;
        }
        b.m[w++] = b.m[i];
    }
    b.nm = w;

    /* uadeinfo (R2): index 0 of the format table is the empty format */
    if (pool_ref(&b.pl, "", 1) < 0 || fmt_id(&b, 0) < 0) goto done;
    for (b.pass = 0; b.pass < 2; b.pass++) {
        for (b.is_meta = 0; b.is_meta < 2; b.is_meta++) {
            snprintf(p, sizeof p, "%s/tsv/pretty/%s/%s.tsv", checkout, b.pass == 0 ? "xxh32" : "md5",
                     b.is_meta ? "metadata" : "modinfos");
            if (!each_row(p, info_cb, &b) || b.failed) goto done;
        }
    }
    {
        /* info_entries: entries with a non-empty format (modinfos covers every key) */
        long cnt = 0;
        uint32_t k;
        for (k = 0; k < b.n; k++) cnt += b.info[k].fmt != 0;
        st->info_entries = cnt;
    }
    snprintf(q, sizeof q, "%s/%s", outdir, SEL_UADEINFO_NAME);
    if (!write_info(&b, q, st)) goto done;
    snprintf(q, sizeof q, "%s/%s", outdir, SEL_UADEMD5_NAME);
    ok = write_md5(&b, q, st);
done:
    free(b.keys);
    free(b.info);
    free(b.fmt_ref);
    free(b.pl.buf);
    free(b.pl.tab);
    free(b.m);
    return ok ? 0 : 1;
}
