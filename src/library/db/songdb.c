/* DEViLBOX songdb lookup, composer alias merge and the composer page query (plan 2026-10-25-m3-library.md I182,
 * T85; design doc 9.2 "author normalisation via DEViLBOX songdb").
 *
 * Endpoint (research 2026-10-25-m3-external-endpoints.md, routes/songdb.ts:17):
 *   GET <devilbox>/api/songdb/lookup?hash=<12 hex UADE key>
 *   -> {"found":true,"format":..,"authors":["..",".."],..} | {"found":false}
 * The answer is cached by UADE key in songdb_cache (authors one per line; NULL = not in songdb), so a second
 * run asks nothing. Each author goes through composer.h (case, punctuation, "Last, First") and lands in
 * song_composers(song_key, canon, display); the composer page reads that table by canon. The worker half runs the
 * SEL_SONGDB_* ops; the caller half rides on sel_library_genre_sync, which already holds the UADE key. */
#include "db_internal.h"

#include "../composer.h"
#include "../devilbox_loudness.h"
#include "../net/mirrors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SONGDB_TEXT_MAX 4096u
#define AUTHORS_MAX 1024u
#define URL_MAX 128u

/* ---- worker half ---- */

static int op_get(sqlite3* db, const sel_genre_op* op, int* found) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, "SELECT authors FROM songdb_cache WHERE key = ?1", -1, &st, NULL);

    *found = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const char* a = (const char*)sqlite3_column_text(st, 0);

        snprintf(op->out, op->out_cap, "%s", a != NULL ? a : "");
        *found = 1;
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int op_put(sqlite3* db, const sel_genre_op* op) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO songdb_cache(key, authors, fetched) VALUES(?1, ?2, ?3)", -1,
                                &st, NULL);

    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    if (op->value != NULL) {
        sqlite3_bind_text(st, 2, op->value, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(st, 2);
    }
    sqlite3_bind_int64(st, 3, op->at);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* I296: the scanned file's author tag becomes the composer row of song `key` (tag = 1) until the songdb sync knows the
 * song; a song with songdb rows keeps them. An untagged song gets no row. Same canon/display as op_apply. */
int sel_library_tag_composer(sqlite3* db, const char* key, const char* author) {
    sqlite3_stmt* st = NULL;
    char canon[SEL_COMPOSER_MAX];
    char display[SEL_COMPOSER_MAX];
    int rc;

    rc = sqlite3_prepare_v2(db, "DELETE FROM song_composers WHERE song_key = ?1 AND tag = 1", -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return rc;
    if (author == NULL || !sel_composer_canon(author, canon, sizeof canon) ||
        !sel_composer_display(author, display, sizeof display)) {
        return SQLITE_OK;
    }
    st = NULL;
    rc = sqlite3_prepare_v2(db,
                            "INSERT OR IGNORE INTO song_composers(song_key, canon, display, tag)"
                            " SELECT ?1, ?2, ?3, 1 WHERE NOT EXISTS (SELECT 1 FROM song_composers WHERE song_key = ?1)",
                            -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, canon, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, display, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* Replaces the composer rows of song `op->key` with one row per author line of `op->value`. found = rows kept.
 * Tag rows (I296) go only when songdb knows the song; an unknown song keeps its tag row. */
static int op_apply(sqlite3* db, const sel_genre_op* op, int* found) {
    sqlite3_stmt* st = NULL;
    const char* p = op->value != NULL ? op->value : "";
    int rc;

    *found = 0;
    rc = sqlite3_prepare_v2(db, "DELETE FROM song_composers WHERE song_key = ?1 AND tag = 0", -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return rc;
    while (*p != '\0') {
        char line[SEL_COMPOSER_MAX];
        char canon[SEL_COMPOSER_MAX];
        char display[SEL_COMPOSER_MAX];
        size_t n = strcspn(p, "\n");

        snprintf(line, sizeof line, "%.*s", (int)(n < sizeof line - 1u ? n : sizeof line - 1u), p);
        p += n;
        if (*p == '\n') p++;
        if (!sel_composer_canon(line, canon, sizeof canon) || !sel_composer_display(line, display, sizeof display)) {
            continue;
        }
        st = NULL;
        rc = sqlite3_prepare_v2(db, "INSERT INTO song_composers(song_key, canon, display, tag) VALUES(?1, ?2, ?3, 0)"
                                " ON CONFLICT(song_key, canon) DO UPDATE SET display = excluded.display, tag = 0", -1,
                                &st, NULL);
        if (rc != SQLITE_OK) return rc;
        sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 2, canon, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 3, display, -1, SQLITE_STATIC);
        rc = sqlite3_step(st);
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) return rc;
        *found = 1;
    }
    if (*found) {
        st = NULL;
        rc = sqlite3_prepare_v2(db, "DELETE FROM song_composers WHERE song_key = ?1 AND tag = 1", -1, &st, NULL);
        if (rc != SQLITE_OK) return rc;
        sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
        rc = sqlite3_step(st);
        sqlite3_finalize(st);
        if (rc != SQLITE_DONE) return rc;
    }
    return SQLITE_OK;
}

static int op_composer_next(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db,
                                "SELECT songs.id, songs.key, COALESCE(songs.name, ''), COALESCE(c.display, '"
                                SEL_COMPOSER_UNKNOWN_DISPLAY "') FROM songs"
                                " LEFT JOIN song_composers c ON c.song_key = songs.key"
                                " WHERE (c.canon = ?1 OR (?1 = '" SEL_COMPOSER_UNKNOWN_CANON "' AND c.song_key IS NULL))"
                                " AND songs.id > ?2 ORDER BY songs.id LIMIT 1",
                                -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, op->after);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const char* key = (const char*)sqlite3_column_text(st, 1);
        const char* name = (const char*)sqlite3_column_text(st, 2);
        const char* disp = (const char*)sqlite3_column_text(st, 3);
        size_t kl = strlen(key), nl = strlen(name), dl = strlen(disp);

        if (kl + nl + dl + 3u <= op->out_cap) {
            memcpy(op->out, key, kl + 1u);
            memcpy(op->out + kl + 1u, name, nl + 1u);
            memcpy(op->out + kl + nl + 2u, disp, dl + 1u);
            *id = sqlite3_column_int64(st, 0);
        }
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* SEL_COMPOSER_LIST: every composer, one row per canonical spelling with its distinct song count, streamed through op->row in one pass (I528: a per-row OFFSET re-ran the whole GROUP BY for each composer). */
static int op_composer_list(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    static const char* const sql[2] = {
        "SELECT canon, display, songs FROM composers ORDER BY display, canon",
        "SELECT '" SEL_COMPOSER_UNKNOWN_CANON "', '" SEL_COMPOSER_UNKNOWN_DISPLAY "', n FROM composer_unknown WHERE n > 0"};
    int64_t rows = 0;
    int rc = SQLITE_DONE, part, stop = 0;

    *id = 0;
    for (part = 0; part < 2 && !stop; part++) { /* the Unknown composer group comes last */
        sqlite3_stmt* st = NULL;

        rc = sqlite3_prepare_v2(db, sql[part], -1, &st, NULL);
        if (rc != SQLITE_OK) return rc;
        while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
            char num[16];
            const char* f[3];

            snprintf(num, sizeof num, "%d", sqlite3_column_int(st, 2));
            f[0] = (const char*)sqlite3_column_text(st, 0);
            f[1] = (const char*)sqlite3_column_text(st, 1);
            f[2] = num;
            rows++;
            if (op->row(op->row_ctx, f) != 0) {
                stop = 1;
                break;
            }
        }
        sqlite3_finalize(st);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) return rc;
    }
    *id = rows;
    return SQLITE_OK;
}

/* SEL_SOURCE_SONGS: how many songs rows carry source id `key`, as decimal text in `out`. The user DB part reads the
 * trigger-kept source_songs count (schema v8), never a scan of songs. */
static int op_source_songs(sqlite3* db, int has_index, const sel_genre_op* op, int* found) {
    sqlite3_stmt* st = NULL;
    /* An attached index adds the songs it holds that the user DB does not (the same key counts once, I265). */
    int rc = sqlite3_prepare_v2(db,
                                has_index ? "SELECT IFNULL((SELECT n FROM source_songs WHERE source = ?1), 0) + (SELECT COUNT(*) FROM"
                                            " idx.songs AS i WHERE i.source = ?1 AND NOT EXISTS (SELECT 1 FROM songs AS s"
                                            " WHERE s.key = i.key))"
                                          : "SELECT IFNULL((SELECT n FROM source_songs WHERE source = ?1), 0)",
                                -1, &st, NULL);

    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        char num[16];
        snprintf(num, sizeof num, "%d", sqlite3_column_int(st, 0));
        if (strlen(num) + 1u <= op->out_cap) {
            memcpy(op->out, num, strlen(num) + 1u);
            *found = 1;
        }
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int sel_library_songdb_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    int rc = SQLITE_MISUSE;

    *found = 0;
    *id = 0;
    if (op->op == SEL_SONGDB_GET && op->out != NULL && op->out_cap > 0) {
        rc = op_get(lib->db, op, found);
    } else if (op->op == SEL_SONGDB_PUT) {
        rc = op_put(lib->db, op);
        *found = rc == SQLITE_OK;
    } else if (op->op == SEL_SONGDB_APPLY) {
        rc = op_apply(lib->db, op, found);
    } else if (op->op == SEL_COMPOSER_NEXT && op->out != NULL) {
        rc = op_composer_next(lib->db, op, id);
        *found = *id != 0;
    } else if (op->op == SEL_COMPOSER_LIST && op->row != NULL) {
        rc = op_composer_list(lib->db, op, id);
        *found = *id != 0;
    } else if (op->op == SEL_SOURCE_SONGS && op->key != NULL && op->out != NULL) {
        rc = op_source_songs(lib->db, lib->has_index, op, found);
    }
    return rc;
}

/* ---- caller half ---- */

/* Copies the strings of the `"authors":[..]` array into `out`, one per line. 1 when the array exists. */
static int json_authors(const char* json, char* out, size_t cap) {
    const char* p = strstr(json, "\"authors\"");
    size_t n = 0;

    out[0] = '\0';
    if (p == NULL) return 0;
    p += strlen("\"authors\"");
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '[') return 0;
    for (p++; *p != '\0' && *p != ']'; p++) {
        if (*p != '"') continue;
        if (n > 0 && n + 1 < cap) out[n++] = '\n';
        for (p++; *p != '\0' && *p != '"'; p++) {
            char c = *p;

            if (c == '\\' && p[1] != '\0') c = *++p;
            if (c == '\n') c = ' ';
            if (n + 1 < cap) out[n++] = c;
        }
        if (*p == '\0') break;
    }
    out[n] = '\0';
    return 1;
}

/* Asks the endpoint. The route takes the first 12 lower-case hex characters of the md5 of the file bytes (server
 * routes/songdb.ts), not the uade key; the uade key stays the cache and vote key. 1 when the reply is final (authors in
 * `authors`, "" = not in songdb), 0 when unknown (including an unreadable file). */
static int songdb_lookup(const char* path, char* authors, sel_genre_stats* st) {
    char url[URL_MAX + 64u];
    char text[SONGDB_TEXT_MAX];
    char md5[33];

    authors[0] = '\0';
    if (!sel_dvb_loudness_file_hash(path, md5)) return 0;
    snprintf(url, sizeof url, "%s/api/songdb/lookup?hash=%.12s", sel_devilbox_base(), md5);
    st->songdb_lookups++;
    authors[0] = '\0';
    if (!sel_library_http_json(url, text, sizeof text, &st->http_requests)) return 0;
    if (strstr(text, "\"found\":true") != NULL || strstr(text, "\"found\": true") != NULL) {
        json_authors(text, authors, AUTHORS_MAX);
    }
    return 1;
}

int sel_library_songdb_resolve(sel_library* lib, const char* ukey, const char* path, const char* song_key,
                               sel_genre_stats* stats) {
    char authors[AUTHORS_MAX];
    sel_genre_op op;
    int cached;

    memset(&op, 0, sizeof op);
    op.op = SEL_SONGDB_GET;
    op.key = ukey;
    op.out = authors;
    op.out_cap = sizeof authors;
    cached = sel_genre_call(lib, &op, NULL);
    if (cached < 0) return 0;
    if (cached == 1) {
        stats->songdb_cache_hits++;
    } else if (!songdb_lookup(path, authors, stats)) {
        return 1; /* 503, 404, transport: unknown, nothing cached, not an error */
    } else {
        memset(&op, 0, sizeof op);
        op.op = SEL_SONGDB_PUT;
        op.key = ukey;
        op.value = authors[0] != '\0' ? authors : NULL;
        op.at = (int64_t)time(NULL);
        if (sel_genre_call(lib, &op, NULL) < 0) return 0;
    }
    memset(&op, 0, sizeof op);
    op.op = SEL_SONGDB_APPLY;
    op.key = song_key;
    op.value = authors;
    {
        int applied = sel_genre_call(lib, &op, NULL);

        if (applied < 0) return 0;
        if (applied == 1) stats->composers_set++;
    }
    return 1;
}

int sel_library_composer_page(sel_library* lib, const char* name, sel_composer_row_fn fn, void* ctx) {
    char canon[SEL_COMPOSER_MAX];
    char buf[SEL_COMPOSER_MAX * 2u + 1100u];
    int64_t after = 0, id = 0;
    int rows = 0;

    if (lib == NULL) return -1;
    if (strcmp(name, SEL_COMPOSER_UNKNOWN_CANON) == 0) { /* the group of songs without a composer: no spelling to normalise */
        snprintf(canon, sizeof canon, "%s", name);
    } else if (!sel_composer_canon(name, canon, sizeof canon)) {
        return -1;
    }
    for (;;) {
        sel_genre_op op;
        const char* name_text;
        const char* disp;

        memset(&op, 0, sizeof op);
        op.op = SEL_COMPOSER_NEXT;
        op.key = canon;
        op.after = after;
        op.out = buf;
        op.out_cap = sizeof buf;
        if (sel_genre_call(lib, &op, &id) < 0) return -1;
        if (id == 0) break;
        after = id;
        name_text = buf + strlen(buf) + 1u;
        disp = name_text + strlen(name_text) + 1u;
        if (fn != NULL) fn(ctx, buf, name_text, disp);
        rows++;
    }
    return rows;
}

typedef struct composer_sink {
    sel_composer_list_fn fn;
    void* ctx;
} composer_sink;

static int composer_row(void* ctx, const char* const* f) {
    composer_sink* k = ctx;

    if (k->fn != NULL) k->fn(k->ctx, f[0], f[1], (uint32_t)strtoul(f[2], NULL, 10));
    return 0;
}

int sel_library_composer_list(sel_library* lib, sel_composer_list_fn fn, void* ctx) {
    composer_sink k;
    sel_genre_op op;
    int64_t rows = 0;

    if (lib == NULL) return -1;
    k.fn = fn;
    k.ctx = ctx;
    memset(&op, 0, sizeof op);
    op.op = SEL_COMPOSER_LIST;
    op.row = composer_row;
    op.row_ctx = &k;
    if (sel_genre_call(lib, &op, &rows) < 0) return -1;
    return (int)rows;
}

int sel_library_source_songs(sel_library* lib, const char* source) {
    char buf[16];
    sel_genre_op op;

    if (lib == NULL || source == NULL) return -1;
    memset(&op, 0, sizeof op);
    op.op = SEL_SOURCE_SONGS;
    op.key = source;
    op.out = buf;
    op.out_cap = sizeof buf;
    if (sel_genre_call(lib, &op, NULL) != 1) return -1;
    return (int)strtol(buf, NULL, 10);
}
