/* Library search on the worker (plan Phase 2, I149 and I150): LIBRARY_SEARCH, saved views and the
 * TSV catalogue import. Query text becomes SQL only through sel_query_compile; bare words are
 * served by the FTS5 index (prefix match per word), everything else by the compiled SQL with
 * keys sorted page by page (I198a). REGEXP is a SQL function backed by src/library/regex.c, so `key:/re/`
 * runs the same on the desktop and the 68k. */
#include "db_internal.h"

#include "../regex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void regexp_fn(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    const char* pat = (const char*)sqlite3_value_text(argv[0]);
    const char* txt = (const char*)sqlite3_value_text(argv[1]);
    int r;

    (void)argc;
    if (pat == NULL || txt == NULL) {
        sqlite3_result_null(ctx);
        return;
    }
    r = sel_regex_match(pat, txt);
    if (r < 0) {
        sqlite3_result_error(ctx, "bad_regex", -1);
        return;
    }
    sqlite3_result_int(ctx, r);
}

/* Long FTS prefixes (I198b). The index has prefix lengths 2 and 3; a longer prefix makes FTS5 merge every
 * matching doclist into one buffer (fts5SetupPrefixIter), which does not fit the 512 KiB heap. So a word
 * of 4+ letters searches the streamed 3-letter prefix, and sel_fts_prefix() re-checks the full word against
 * the row's tokens. The check tokenizes with FTS5's own unicode61 tokenizer, so folding matches the index. */
#define FTS_WORDS 8
#define FTS_WORD_MAX 48

typedef struct {
    fts5_api* api;
    fts5_tokenizer tm;
    Fts5Tokenizer* tok;
} fts_tok;

typedef struct {
    int n;
    int len[FTS_WORDS];
    char w[FTS_WORDS][FTS_WORD_MAX];
} fts_words;

typedef struct {
    const fts_words* words;
    unsigned seen; /* bit per word already found as a prefix of a token */
    fts_words* collect;
} fts_scan;

static int fts_word_cb(void* c, int flags, const char* tok, int n, int start, int end) {
    fts_scan* sc = (fts_scan*)c;
    int i;

    (void)flags;
    (void)start;
    (void)end;
    if (sc->collect != NULL) {
        fts_words* w = sc->collect;
        if (w->n < FTS_WORDS && n < FTS_WORD_MAX) {
            memcpy(w->w[w->n], tok, (size_t)n);
            w->len[w->n++] = n;
        }
        return SQLITE_OK;
    }
    for (i = 0; i < sc->words->n; i++) {
        if (n >= sc->words->len[i] && memcmp(tok, sc->words->w[i], (size_t)sc->words->len[i]) == 0) {
            sc->seen |= 1u << i;
        }
    }
    return sc->seen == (1u << sc->words->n) - 1u ? SQLITE_DONE : SQLITE_OK;
}

static int fts_tokenize(fts_tok* t, fts_scan* sc, const char* text) {
    int rc = t->tm.xTokenize(t->tok, sc, FTS5_TOKENIZE_DOCUMENT, text, (int)strlen(text), fts_word_cb);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static void fts_prefix_fn(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    fts_tok* t = (fts_tok*)sqlite3_user_data(ctx);
    fts_words* w = (fts_words*)sqlite3_get_auxdata(ctx, 0);
    fts_scan sc;
    int i;

    if (w == NULL) {
        const char* q = (const char*)sqlite3_value_text(argv[0]);
        w = (fts_words*)sqlite3_malloc((int)sizeof *w);
        if (w == NULL || q == NULL) {
            sqlite3_free(w);
            sqlite3_result_error_nomem(ctx);
            return;
        }
        memset(w, 0, sizeof *w);
        memset(&sc, 0, sizeof sc);
        sc.collect = w;
        if (fts_tokenize(t, &sc, q) != SQLITE_OK) {
            sqlite3_free(w);
            sqlite3_result_error(ctx, "fts_tokenize", -1);
            return;
        }
        sqlite3_set_auxdata(ctx, 0, w, sqlite3_free);
    }
    memset(&sc, 0, sizeof sc);
    sc.words = w;
    for (i = 1; i < argc && sc.seen != (1u << w->n) - 1u; i++) {
        const char* col = (const char*)sqlite3_value_text(argv[i]);
        if (col != NULL && fts_tokenize(t, &sc, col) != SQLITE_OK) {
            sqlite3_result_error(ctx, "fts_tokenize", -1);
            return;
        }
    }
    sqlite3_result_int(ctx, sc.seen == (1u << w->n) - 1u);
}

static void fts_tok_free(void* p) {
    fts_tok* t = (fts_tok*)p;
    if (t->tok != NULL) {
        t->tm.xDelete(t->tok);
    }
    sqlite3_free(t);
}

/* fts5_api through `SELECT fts5(?1)` with a pointer binding, then the unicode61 tokenizer. */
static int fts_open(sqlite3* db, fts_tok* t) {
    sqlite3_stmt* st = NULL;
    void* ctx = NULL;

    memset(t, 0, sizeof *t);
    if (sqlite3_prepare_v2(db, "SELECT fts5(?1)", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_pointer(st, 1, (void*)&t->api, "fts5_api_ptr", NULL);
        sqlite3_step(st);
    }
    sqlite3_finalize(st);
    return t->api != NULL && t->api->xFindTokenizer(t->api, "unicode61", &ctx, &t->tm) == SQLITE_OK &&
           t->tm.xCreate(ctx, NULL, 0, &t->tok) == SQLITE_OK;
}

static int register_fts_prefix(sqlite3* db) {
    fts_tok* t = (fts_tok*)sqlite3_malloc((int)sizeof *t);

    if (t == NULL) {
        return 0;
    }
    if (fts_open(db, t)) {
        return sqlite3_create_function_v2(db, "sel_fts_prefix", -1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, t,
                                          fts_prefix_fn, NULL, NULL, fts_tok_free) == SQLITE_OK;
    }
    sqlite3_free(t);
    return 0;
}

/* The tokens unicode61 makes of a query word (I198d). A non-ASCII word folds (e with an acute accent becomes e),
 * so its length is the length of the folded token in characters, not the bytes typed; a word of punctuation
 * only makes no token at all. The same tokenizer as the index and sel_fts_prefix(), so the three agree. */
static int fts_fold_word(sqlite3* db, const char* w, size_t len, fts_words* out) {
    fts_tok t;
    fts_scan sc;
    int rc;

    memset(out, 0, sizeof *out);
    if (!fts_open(db, &t)) {
        return 0;
    }
    memset(&sc, 0, sizeof sc);
    sc.collect = out;
    rc = t.tm.xTokenize(t.tok, &sc, FTS5_TOKENIZE_DOCUMENT, w, (int)len, fts_word_cb);
    t.tm.xDelete(t.tok);
    return rc == SQLITE_OK || rc == SQLITE_DONE;
}

/* Characters (UTF-8 code points) in n bytes. */
static int utf8_chars(const char* s, int n) {
    int c = 0, i;

    for (i = 0; i < n; i++) {
        c += ((unsigned char)s[i] & 0xC0) != 0x80;
    }
    return c;
}

/* Bytes of the first k characters of s (n bytes). */
static int utf8_prefix_bytes(const char* s, int n, int k) {
    int i = 0;

    while (i < n && k > 0) {
        i++;
        while (i < n && ((unsigned char)s[i] & 0xC0) == 0x80) {
            i++;
        }
        k--;
    }
    return i;
}

int sel_library_register_regexp(sqlite3* db) {
    return sqlite3_create_function(db, "regexp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC, NULL, regexp_fn, NULL,
                                   NULL) == SQLITE_OK &&
           register_fts_prefix(db);
}

/* Saved query lookup for `view:name`: the text lives in a per-compile slot, because the compiler
 * keeps reading the text of every view on its expansion stack. */
static const char* view_lookup(void* ctx, const char* name) {
    sel_library* lib = ctx;
    sqlite3_stmt* st = NULL;
    const char* text = NULL;

    if (lib->view_used >= sizeof lib->view_text / sizeof lib->view_text[0]) {
        return NULL;
    }
    if (sqlite3_prepare_v2(lib->db, "SELECT query FROM views WHERE name = ?1", -1, &st, NULL) != SQLITE_OK) {
        return NULL;
    }
    sqlite3_bind_text(st, 1, name, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) {
        char* slot = lib->view_text[lib->view_used];
        const char* q = (const char*)sqlite3_column_text(st, 0);
        if (q != NULL && strlen(q) < sizeof lib->view_text[0]) {
            strcpy(slot, q);
            lib->view_used++;
            text = slot;
        }
    }
    sqlite3_finalize(st);
    return text;
}

static void set_error(sel_lib_job* job, const char* code) {
    strncpy(job->error, code, sizeof job->error - 1u);
    job->error[sizeof job->error - 1u] = '\0';
}

/* Maps the last SQLite failure on `db` to the engine's error vocabulary: an exhausted library heap is
 * out_of_memory, never bad_value (which stays for a statement SQLite rejects). */
static void set_sql_error(sel_lib_job* job, sqlite3* db) {
    int rc = sqlite3_errcode(db);

    set_error(job, (rc & 0xff) == SQLITE_NOMEM ? "out_of_memory" : "bad_value");
}

/* Bare words: letters, digits, `_`, non-ASCII (UTF-8) characters and blanks only, and none of the query operators. */
static int word_is_ascii(const char* w, size_t n) {
    size_t i;

    for (i = 0; i < n; i++) {
        if ((unsigned char)w[i] >= 0x80) {
            return 0;
        }
    }
    return 1;
}

static int is_bare_words(const char* t) {
    int words = 0;
    int in_word = 0;
    const char* w = t;

    for (; *t != '\0'; t++) {
        char c = *t;
        int alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                    (unsigned char)c >= 0x80;
        if (alnum) {
            if (!in_word) {
                w = t;
                words++;
            }
            in_word = 1;
        } else if (c == ' ') {
            if (in_word && ((t - w == 2 && strncmp(w, "or", 2) == 0) || (t - w == 3 && (strncmp(w, "and", 3) == 0 || strncmp(w, "not", 3) == 0)))) {
                return 0;
            }
            in_word = 0;
        } else {
            return 0;
        }
    }
    if (in_word && ((t - w == 2 && strncmp(w, "or", 2) == 0) || (t - w == 3 && (strncmp(w, "and", 3) == 0 || strncmp(w, "not", 3) == 0)))) {
        return 0;
    }
    return words > 0;
}

/* A query of punctuation only (no letter, digit, non-ASCII character, quote, bracket, `:` or `/`) tokenizes to
 * nothing, so no row can match it (I198d). */
static int punct_only(const char* t) {
    int any = 0;

    for (; *t != '\0'; t++) {
        unsigned char c = (unsigned char)*t;
        if (c == ' ') {
            continue;
        }
        if (c >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
            c == '"' || c == '(' || c == ')' || c == ':' || c == '/') {
            return 0;
        }
        any = 1;
    }
    return any;
}

/* Sorted hits come in pages (I198a): `ORDER BY key LIMIT n` keeps at most n rows in the sorter, and the
 * next page starts after the last key copied. One sort over every hit holds all of them in the SQLite
 * heap, and a broad search over the attached index runs out of the 512 KiB heap of a 2 MB machine.
 * SQLite does not push the LIMIT of a UNION ALL into its arms (it sorts each arm whole, then merges), so
 * the user DB and the index run as two paged statements and collect() merges their sorted keys. */
static int page_rows(const sel_library* lib) {
    uint32_t n = lib->heap_bytes / 2048u;
    return n < 64u ? 64 : n > 16384u ? 16384 : (int)n;
}

/* Binds the FTS match text, or the compiled query's parameters. */
static void bind_query(sel_library* lib, sqlite3_stmt* st, const char* match) {
    unsigned i;

    if (match != NULL) {
        int px = sqlite3_bind_parameter_index(st, ":pfx");
        sqlite3_bind_text(st, 1, match, -1, SQLITE_STATIC);
        if (px > 0) {
            sqlite3_bind_text(st, px, lib->fts_long, -1, SQLITE_STATIC);
        }
        return;
    }
    for (i = 0; i < lib->query.nparams; i++) {
        const sel_query_param* p = &lib->query.params[i];
        if (p->is_int) {
            sqlite3_bind_int64(st, (int)i + 1, p->i);
        } else {
            sqlite3_bind_text(st, (int)i + 1, p->s, -1, SQLITE_STATIC);
        }
    }
}

/* Prepares `sql` with `arm` (one column named key) in its one %s and binds the query. NULL on an error. */
static sqlite3_stmt* prepare_arm(sel_library* lib, const char* sql, const char* arm, const char* match,
                                 sel_lib_job* job) {
    sqlite3_stmt* st = NULL;
    char* text = sqlite3_mprintf(sql, arm);

    if (text == NULL || sqlite3_prepare_v2(lib->db, text, -1, &st, NULL) != SQLITE_OK) {
        set_sql_error(job, lib->db);
        sqlite3_finalize(st);
        st = NULL;
    } else {
        bind_query(lib, st, match);
    }
    sqlite3_free(text);
    return st;
}

/* Adds the hits of `arm` after `after` to job->truncated: the rows of a search whose keys did not fit. */
static void count_rest(sel_library* lib, const char* arm, const char* match, const char* after, sel_lib_job* job) {
    sqlite3_stmt* st = prepare_arm(lib, "SELECT count(*) FROM (%s) WHERE key > :after", arm, match, job);

    if (st == NULL) {
        return;
    }
    sqlite3_bind_text(st, sqlite3_bind_parameter_index(st, ":after"), after, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) {
        job->truncated += (uint32_t)sqlite3_column_int64(st, 0);
    } else {
        set_sql_error(job, lib->db);
    }
    sqlite3_finalize(st);
}

/* One paged statement over one schema. `after` is the last key taken from it (a copy in job->out). */
typedef struct {
    const char* arm;
    sqlite3_stmt* st;
    const char* after;
    int got;  /* rows of the current page stepped */
    int live; /* st holds a row */
} sel_page_arm;

/* Steps `a` to its next row, starting the next page when a full page ran out. 0 on an error. */
static int arm_step(sel_library* lib, sel_page_arm* a, int rows, sel_lib_job* job) {
    for (;;) {
        int rc = sqlite3_step(a->st);
        if (rc == SQLITE_ROW) {
            a->got++;
            a->live = 1;
            return 1;
        }
        a->live = 0;
        if (rc != SQLITE_DONE) {
            set_sql_error(job, lib->db);
            return 0;
        }
        if (a->got < rows) {
            return 1;
        }
        sqlite3_reset(a->st);
        sqlite3_bind_text(a->st, sqlite3_bind_parameter_index(a->st, ":after"), a->after, -1, SQLITE_STATIC);
        a->got = 0;
    }
}

void sel_library_set_sources_off(sel_library* lib, const char* csv) {
    if (lib == NULL) return;
    /* one writer (the engine thread), read by the worker: a torn read only skips or admits one row for one search */
    snprintf(lib->src_off, sizeof lib->src_off, "%s", csv != NULL ? csv : "");
}

int sel_library_csv_has(const char* csv, const char* id) {
    const char* p;
    size_t n;
    if (csv == NULL || id == NULL || id[0] == '\0') return 0;
    n = strlen(id);
    for (p = csv; *p != '\0';) {
        const char* end = strchr(p, ',');
        size_t len = end != NULL ? (size_t)(end - p) : strlen(p);
        if (len == n && strncmp(p, id, n) == 0) return 1;
        p += len;
        if (*p == ',') p++;
    }
    return 0;
}

int sel_library_key_source_off(sel_library* lib, const char* key) {
    char src[32];
    const char* colon;
    if (lib == NULL || lib->src_off[0] == '\0' || key == NULL) return 0;
    colon = strchr(key, ':');
    if (colon == NULL || (size_t)(colon - key) >= sizeof src) return 0;
    memcpy(src, key, (size_t)(colon - key));
    src[colon - key] = '\0';
    return sel_library_csv_has(lib->src_off, src);
}

/* Copies the sorted keys of `arm` (and of `arm_idx`, the same query over the attached index) into job->out
 * until they run out or one does not fit. A key equal to the one before it (the same song in the user DB
 * and the index) is copied once. */
static void collect(sel_library* lib, const char* arm, const char* arm_idx, const char* match, uint32_t max_hits,
                    sel_lib_job* job) {
    static const char PAGE[] = "SELECT key FROM (%s) WHERE key > :after ORDER BY key LIMIT :rows";
    sel_page_arm arms[2];
    const char* last = NULL; /* the last key copied into job->out */
    unsigned narms = arm_idx != NULL ? 2u : 1u, i;
    uint32_t used = 0;
    int rows = page_rows(lib), ok = 1;

    memset(arms, 0, sizeof arms);
    arms[0].arm = arm;
    arms[1].arm = arm_idx;
    for (i = 0; i < narms && ok; i++) {
        arms[i].after = "";
        arms[i].st = prepare_arm(lib, PAGE, arms[i].arm, match, job);
        if (arms[i].st == NULL) {
            ok = 0;
            break;
        }
        sqlite3_bind_text(arms[i].st, sqlite3_bind_parameter_index(arms[i].st, ":after"), "", -1, SQLITE_STATIC);
        sqlite3_bind_int(arms[i].st, sqlite3_bind_parameter_index(arms[i].st, ":rows"), rows);
        ok = arm_step(lib, &arms[i], rows, job);
    }
    while (ok && (arms[0].live || arms[1].live)) {
        sel_page_arm* t = &arms[0];
        const char* k;
        size_t n;
        if (!arms[0].live ||
            (arms[1].live && strcmp((const char*)sqlite3_column_text(arms[1].st, 0),
                                    (const char*)sqlite3_column_text(arms[0].st, 0)) < 0)) {
            t = &arms[1];
        }
        k = (const char*)sqlite3_column_text(t->st, 0);
        n = k != NULL ? strlen(k) + 1u : 0;
        if (n > 0 && sel_library_key_source_off(lib, k)) n = 0; /* a switched-off source (I265) */
        if (n > 0 && (last == NULL || strcmp(k, last) != 0)) {
            if (max_hits > 0 && job->count >= max_hits) {
                /* A per-row scan stops here: counting the rest would walk every row again (I198e). */
                job->truncated = 1;
                break;
            }
            if (used + n > job->out_cap) {
                /* the rest of each arm after its last key taken, this key included */
                for (i = 0; i < narms; i++) {
                    count_rest(lib, arms[i].arm, match, arms[i].after, job);
                }
                break;
            }
            memcpy(job->out + used, k, n);
            last = job->out + used;
            used += (uint32_t)n;
            job->count++;
        }
        if (last != NULL) {
            t->after = last; /* this key, or the equal one copied before it */
        }
        ok = arm_step(lib, t, rows, job);
    }
    for (i = 0; i < narms; i++) {
        sqlite3_finalize(arms[i].st);
    }
}

/* Fills job->meta with title, artist, format, duration and album for each key collected into job->out.
 * A row that does not fit, or has no match, gets five empty fields so the rows stay aligned. */
static void collect_meta(sel_library* lib, sel_lib_job* job) {
    sqlite3_stmt* st = NULL;
    const char* key = job->out;
    uint32_t used = 0, i;

    job->meta_used = 0;
    if (job->meta == NULL || job->meta_cap == 0) {
        return;
    }
    if (sqlite3_prepare_v2(lib->db,
                           lib->has_index
                               ? "SELECT name, artist, format, duration_ms, album FROM songs WHERE key = ?1"
                                 " UNION ALL SELECT name, artist, format, duration_ms, album FROM idx.songs WHERE key = ?1"
                               : "SELECT name, artist, format, duration_ms, album FROM songs WHERE key = ?1",
                           -1, &st, NULL) != SQLITE_OK) {
        st = NULL;
    }
    for (i = 0; i < job->count; i++) {
        const char* f[3] = {"", "", ""}, *album = "";
        char dur[32] = ""; /* two long long fields: 19 + 1 + 19 digits would not fit 16 */
        size_t need;
        int k;
        if (st != NULL) {
            sqlite3_reset(st);
            sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
            if (sqlite3_step(st) == SQLITE_ROW) {
                for (k = 0; k < 3; k++) {
                    const char* t = (const char*)sqlite3_column_text(st, k);
                    f[k] = t != NULL ? t : "";
                }
                album = (const char*)sqlite3_column_text(st, 4);
                album = album != NULL ? album : "";
                if (sqlite3_column_int64(st, 3) > 0) {
                    long long s = sqlite3_column_int64(st, 3) / 1000;
                    snprintf(dur, sizeof dur, "%lld:%02lld", s / 60, s % 60);
                }
            }
        }
        need = strlen(f[0]) + strlen(f[1]) + strlen(f[2]) + strlen(dur) + strlen(album) + 5u;
        if (used + need > job->meta_cap) {
            if (used + 5u > job->meta_cap) {
                break;
            }
            f[0] = f[1] = f[2] = album = "";
            dur[0] = '\0';
            need = 5u;
        }
        for (k = 0; k < 3; k++) {
            size_t n = strlen(f[k]) + 1u;
            memcpy(job->meta + used, f[k], n);
            used += (uint32_t)n;
        }
        memcpy(job->meta + used, dur, strlen(dur) + 1u);
        used += (uint32_t)strlen(dur) + 1u;
        memcpy(job->meta + used, album, strlen(album) + 1u);
        used += (uint32_t)strlen(album) + 1u;
        key += strlen(key) + 1u;
        job->meta_used = used;
    }
    sqlite3_finalize(st);
}

void sel_library_search(sel_library* lib, sel_lib_job* job) {
    char sql_idx[SEL_QUERY_SQL_MAX + 64]; /* the compiled query over the attached index */
    char match[SEL_QUERY_TEXT_MAX * 2];
    static const char CHECK[] = " AND sel_fts_prefix(:pfx, songs.name, songs.artist, songs.format, songs.path)";
    char arm[256], arm_idx[256];
    size_t nlong = 0, nterms = 0;

    job->count = job->truncated = job->used_fts = 0;
    job->error[0] = '\0';
    if (punct_only(job->text)) {
        collect_meta(lib, job);
        return;
    }
    if (is_bare_words(job->text)) {
        const char* p = job->text;
        size_t o = 0;
        lib->fts_long[0] = '\0';
        while (*p != '\0' && o + 6 < sizeof match) {
            const char* w = p;
            size_t len, keep;
            if (*p == ' ') {
                match[o++] = *p++;
                continue;
            }
            while (*p != '\0' && *p != ' ') {
                p++;
            }
            len = (size_t)(p - w);
            if (len > sizeof match - o - 6) {
                len = sizeof match - o - 6;
            }
            keep = len;
            if (!word_is_ascii(w, len)) {
                /* Non-ASCII: the folded tokens decide (I198d). One character after folding is a per-row check,
                 * as for one ASCII letter; longer than 3 searches the first 3 characters of the token. No token
                 * at all (symbols) drops the word. */
                fts_words fw;
                int k;
                if (!fts_fold_word(lib->db, w, len, &fw)) {
                    fw.n = 0;
                }
                for (k = 0; k < fw.n; k++) {
                    int nc = utf8_chars(fw.w[k], fw.len[k]);
                    int kb = nc > 3 ? utf8_prefix_bytes(fw.w[k], fw.len[k], 3) : fw.len[k];
                    if ((nc == 1 || nc > 3) && nlong + (size_t)fw.len[k] + 1 < sizeof lib->fts_long) {
                        memcpy(lib->fts_long + nlong, fw.w[k], (size_t)fw.len[k]);
                        nlong += (size_t)fw.len[k];
                        lib->fts_long[nlong++] = ' ';
                        lib->fts_long[nlong] = '\0';
                    }
                    if (nc != 1 && o + (size_t)kb + 4 < sizeof match) {
                        match[o++] = '"';
                        memcpy(match + o, fw.w[k], (size_t)kb);
                        o += (size_t)kb;
                        match[o++] = '"';
                        match[o++] = '*';
                        match[o++] = ' ';
                        nterms++;
                    }
                }
                continue;
            }
            if (len == 1 && nlong + 2 < sizeof lib->fts_long) {
                /* No prefix index covers one letter: FTS5 would merge every doclist of the letter in memory.
                 * The word leaves the MATCH and sel_fts_prefix() checks it against each row's tokens (I198c). */
                memcpy(lib->fts_long + nlong, w, len);
                nlong += len;
                lib->fts_long[nlong++] = ' ';
                lib->fts_long[nlong] = '\0';
                continue;
            }
            if (len > 3 && memchr(w, '_', len) == NULL && nlong + len + 1 < sizeof lib->fts_long) {
                /* the 3-letter prefix streams from the index; sel_fts_prefix checks the whole word */
                keep = 3;
                memcpy(lib->fts_long + nlong, w, len);
                nlong += len;
                lib->fts_long[nlong++] = ' ';
                lib->fts_long[nlong] = '\0';
            }
            match[o++] = '"';
            memcpy(match + o, w, keep);
            o += keep;
            match[o++] = '"';
            match[o++] = '*';
            nterms++;
        }
        match[o] = '\0';
        if (nterms == 0 && nlong == 0) {
            /* every word tokenized to nothing: no row can match, and an empty MATCH is a syntax error */
            collect_meta(lib, job);
            return;
        }
        job->used_fts = 1;
        snprintf(arm, sizeof arm,
                 "SELECT songs.key AS key FROM song_fts JOIN songs ON songs.id = song_fts.rowid"
                 " WHERE song_fts MATCH ?1%s",
                 nlong > 0 ? CHECK : "");
        snprintf(arm_idx, sizeof arm_idx,
                 "SELECT songs.key AS key FROM idx.song_fts JOIN idx.songs AS songs ON songs.id = song_fts.rowid"
                 " WHERE song_fts MATCH ?1%s",
                 nlong > 0 ? CHECK : "");
        if (nterms == 0) {
            /* only one-letter words: an ordered scan of the songs key, O(page) memory, no FTS */
            snprintf(arm, sizeof arm, "SELECT songs.key AS key FROM songs WHERE 1%s", CHECK);
            snprintf(arm_idx, sizeof arm_idx, "SELECT songs.key AS key FROM idx.songs AS songs WHERE 1%s", CHECK);
            job->used_fts = 0;
        }
        collect(lib, arm, lib->has_index ? arm_idx : NULL, match, nterms == 0 ? job->scan_max : 0, job);
    } else {
        lib->view_used = 0;
        if (!sel_query_compile(job->text, job->now, view_lookup, lib, &lib->query)) {
            set_error(job, lib->query.error);
            return;
        }
        if (lib->has_index) {
            /* The same query once more over the index: only the FROM table changes (the user tables a query
             * joins stay unqualified and resolve to the main DB first). Each copy has its own `?` numbers. */
            const char* where = strstr(lib->query.sql, " WHERE ");
            sqlite3_snprintf((int)sizeof sql_idx, sql_idx, "SELECT songs.key FROM idx.songs AS songs%s",
                             where != NULL ? where : "");
        }
        collect(lib, lib->query.sql, lib->has_index ? sql_idx : NULL, NULL, 0, job);
    }
    collect_meta(lib, job);
}

void sel_library_save_view(sel_library* lib, sel_lib_job* job) {
    sqlite3_stmt* st = NULL;

    job->error[0] = '\0';
    lib->view_used = 0;
    if (!sel_query_compile(job->text2, job->now, view_lookup, lib, &lib->query)) {
        set_error(job, lib->query.error);
        return;
    }
    if (sqlite3_prepare_v2(lib->db, "INSERT OR REPLACE INTO views(name, query) VALUES(?1, ?2)", -1, &st, NULL) !=
        SQLITE_OK) {
        set_error(job, "bad_value");
        return;
    }
    sqlite3_bind_text(st, 1, job->text, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, job->text2, -1, SQLITE_STATIC);
    if (sqlite3_step(st) != SQLITE_DONE) {
        set_error(job, "bad_value");
    }
    sqlite3_finalize(st);
    job->count = 1;
}

/* TSV columns: key, source, path, name, artist, format, year. `#` lines and blank lines are skipped. */
void sel_library_import(sel_library* lib, sel_lib_job* job) {
    sqlite3_stmt* st = NULL;
    const char* p = job->text;

    job->count = 0;
    job->error[0] = '\0';
    if (sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(lib->db,
                           "INSERT INTO songs(key, source, path, name, artist, format, year)"
                           " VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7) ON CONFLICT(key) DO UPDATE SET"
                           " source = excluded.source, path = excluded.path, name = " SEL_SONG_TAG_KEEP(name) ","
                           " artist = " SEL_SONG_TAG_KEEP(artist) ", format = excluded.format, year = excluded.year",
                           -1, &st, NULL) != SQLITE_OK) {
        sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL);
        set_error(job, "bad_value");
        return;
    }
    while (*p != '\0') {
        char line[512];
        size_t n = strcspn(p, "\n");
        char* f[7];
        unsigned nf = 0;
        char* c;
        unsigned i;

        if (n >= sizeof line) {
            p += n + (p[n] == '\n');
            continue;
        }
        memcpy(line, p, n);
        line[n] = '\0';
        p += n + (p[n] == '\n');
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        for (c = line; nf < 7; c++) {
            f[nf++] = c;
            c = strchr(c, '\t');
            if (c == NULL) {
                break;
            }
            *c = '\0';
        }
        if (nf < 7) {
            continue;
        }
        sqlite3_reset(st);
        for (i = 0; i < 6; i++) {
            sqlite3_bind_text(st, (int)i + 1, f[i], -1, SQLITE_STATIC);
        }
        sqlite3_bind_int(st, 7, atoi(f[6]));
        if (sqlite3_step(st) == SQLITE_DONE) {
            job->count++;
        }
    }
    sqlite3_finalize(st);
    sqlite3_exec(lib->db, "COMMIT", NULL, NULL, NULL);
}
