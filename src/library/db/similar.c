/* Similar and also-by queries (plan 2026-10-25-m3-library.md I183, T86; design doc 9 "similar songs, also-by ...
 * are queries over this DB plus DEViLBOX answers cached locally"). Worker half of SEL_SIMILAR_NEXT and
 * SEL_ALSOBY_NEXT, and the caller halves sel_library_similar / sel_library_also_by.
 *
 * Similar: every other song, ordered by genre match with the seed (case-insensitive, an empty genre never
 * matches), then rating (ratings table, unrated = 0, higher first), then library order. Also-by: the other songs
 * that share a canonical composer with the seed (song_composers, filled by the songdb step of the genre sync).
 * Both stream every row through op->row in one request (I528); the reply id is the row count. */
#include "db_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEL_SIMILAR_LIMIT 100

/* Streams the rows of the similar list / also-by list through op->row in one query (I528: a per-row OFFSET re-sorted the library for each row). */
static int op_similar(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int64_t rows = 0;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT s.key, COALESCE(s.name, ''), COALESCE(s.genre, ''), COALESCE(r.rating, 0),"
        " (COALESCE(s.genre, '') <> '' AND lower(s.genre) = lower((SELECT genre FROM songs WHERE key = ?1))) AS m"
        " FROM songs s LEFT JOIN ratings r ON r.key = s.key WHERE s.key <> ?1"
        " ORDER BY m DESC, COALESCE(r.rating, 0) DESC, s.id LIMIT ?2",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, SEL_SIMILAR_LIMIT);
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        char rating[16], match[4];
        const char* f[5];

        snprintf(rating, sizeof rating, "%d", sqlite3_column_int(st, 3));
        snprintf(match, sizeof match, "%d", sqlite3_column_int(st, 4));
        f[0] = (const char*)sqlite3_column_text(st, 0);
        f[1] = (const char*)sqlite3_column_text(st, 1);
        f[2] = (const char*)sqlite3_column_text(st, 2);
        f[3] = rating;
        f[4] = match;
        rows++;
        if (op->row(op->row_ctx, f) != 0) break;
    }
    sqlite3_finalize(st);
    *id = rows;
    return rc == SQLITE_ROW || rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int op_also_by(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int64_t rows = 0;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT s.key, COALESCE(s.name, ''), MIN(c.display) FROM songs s JOIN song_composers c ON c.song_key = s.key"
        " WHERE s.key <> ?1 AND c.canon IN (SELECT canon FROM song_composers WHERE song_key = ?1)"
        " GROUP BY s.key ORDER BY s.id",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        const char* f[3];

        f[0] = (const char*)sqlite3_column_text(st, 0);
        f[1] = (const char*)sqlite3_column_text(st, 1);
        f[2] = (const char*)sqlite3_column_text(st, 2);
        rows++;
        if (op->row(op->row_ctx, f) != 0) break;
    }
    sqlite3_finalize(st);
    *id = rows;
    return rc == SQLITE_ROW || rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int sel_library_similar_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    int rc = SQLITE_MISUSE;

    *found = 0;
    *id = 0;
    if (op->row == NULL || op->key == NULL) return rc;
    if (op->op == SEL_SIMILAR_NEXT) {
        rc = op_similar(lib->db, op, id);
    } else if (op->op == SEL_ALSOBY_NEXT) {
        rc = op_also_by(lib->db, op, id);
    }
    *found = *id != 0;
    return rc;
}

/* ---- caller half ---- */

typedef struct sim_sink {
    sel_similar_row_fn sim;
    sel_composer_row_fn also;
    void* ctx;
} sim_sink;

static int sim_row(void* ctx, const char* const* f) {
    sim_sink* k = ctx;

    if (k->sim != NULL) k->sim(k->ctx, f[0], f[1], f[2], atoi(f[3]), atoi(f[4]));
    return 0;
}

static int also_row(void* ctx, const char* const* f) {
    sim_sink* k = ctx;

    if (k->also != NULL) k->also(k->ctx, f[0], f[1], f[2]);
    return 0;
}

int sel_library_similar(sel_library* lib, const char* song_key, sel_similar_row_fn fn, void* ctx) {
    sim_sink k = {fn, NULL, ctx};
    sel_genre_op op;
    int64_t rows = 0;

    if (lib == NULL || song_key == NULL) return -1;
    memset(&op, 0, sizeof op);
    op.op = SEL_SIMILAR_NEXT;
    op.key = song_key;
    op.row = sim_row;
    op.row_ctx = &k;
    if (sel_genre_call(lib, &op, &rows) < 0) return -1;
    return (int)rows;
}

int sel_library_also_by(sel_library* lib, const char* song_key, sel_composer_row_fn fn, void* ctx) {
    sim_sink k = {NULL, fn, ctx};
    sel_genre_op op;
    int64_t rows = 0;

    if (lib == NULL || song_key == NULL) return -1;
    memset(&op, 0, sizeof op);
    op.op = SEL_ALSOBY_NEXT;
    op.key = song_key;
    op.row = also_row;
    op.row_ctx = &k;
    if (sel_genre_call(lib, &op, &rows) < 0) return -1;
    return (int)rows;
}
