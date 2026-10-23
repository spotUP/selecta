/* Similar and also-by queries (plan 2026-10-25-m3-library.md I183, T86; design doc 9 "similar songs, also-by ...
 * are queries over this DB plus DEViLBOX answers cached locally"). Worker half of SEL_SIMILAR_NEXT and
 * SEL_ALSOBY_NEXT, and the caller halves sel_library_similar / sel_library_also_by.
 *
 * Similar: every other song, ordered by genre match with the seed (case-insensitive, an empty genre never
 * matches), then rating (ratings table, unrated = 0, higher first), then library order. Also-by: the other songs
 * that share a canonical composer with the seed (song_composers, filled by the songdb step of the genre sync).
 * Both are paged by row offset: the worker op returns row `after` and the reply id is after + 1 (0: none left). */
#include "db_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEL_SIMILAR_LIMIT 100

static int pack(const sel_genre_op* op, const char* const* f, int n) {
    size_t total = 0;
    int i;

    for (i = 0; i < n; i++) total += strlen(f[i]) + 1u;
    if (total > op->out_cap) return 0;
    total = 0;
    for (i = 0; i < n; i++) {
        size_t l = strlen(f[i]) + 1u;

        memcpy(op->out + total, f[i], l);
        total += l;
    }
    return 1;
}

static int op_similar(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT s.key, COALESCE(s.name, ''), COALESCE(s.genre, ''), COALESCE(r.rating, 0),"
        " (COALESCE(s.genre, '') <> '' AND lower(s.genre) = lower((SELECT genre FROM songs WHERE key = ?1))) AS m"
        " FROM songs s LEFT JOIN ratings r ON r.key = s.key WHERE s.key <> ?1"
        " ORDER BY m DESC, COALESCE(r.rating, 0) DESC, s.id LIMIT 1 OFFSET ?2",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, op->after);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        char rating[16], match[4];
        const char* f[5];

        snprintf(rating, sizeof rating, "%d", sqlite3_column_int(st, 3));
        snprintf(match, sizeof match, "%d", sqlite3_column_int(st, 4));
        f[0] = (const char*)sqlite3_column_text(st, 0);
        f[1] = (const char*)sqlite3_column_text(st, 1);
        f[2] = (const char*)sqlite3_column_text(st, 2);
        f[3] = rating;
        f[4] = match;
        if (pack(op, f, 5)) *id = op->after + 1;
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int op_also_by(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT s.key, COALESCE(s.name, ''), MIN(c.display) FROM songs s JOIN song_composers c ON c.song_key = s.key"
        " WHERE s.key <> ?1 AND c.canon IN (SELECT canon FROM song_composers WHERE song_key = ?1)"
        " GROUP BY s.key ORDER BY s.id LIMIT 1 OFFSET ?2",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, op->after);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const char* f[3];

        f[0] = (const char*)sqlite3_column_text(st, 0);
        f[1] = (const char*)sqlite3_column_text(st, 1);
        f[2] = (const char*)sqlite3_column_text(st, 2);
        if (pack(op, f, 3)) *id = op->after + 1;
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int sel_library_similar_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    int rc = SQLITE_MISUSE;

    *found = 0;
    *id = 0;
    if (op->out == NULL || op->key == NULL) return rc;
    if (op->op == SEL_SIMILAR_NEXT) {
        rc = op_similar(lib->db, op, id);
    } else if (op->op == SEL_ALSOBY_NEXT) {
        rc = op_also_by(lib->db, op, id);
    }
    *found = *id != 0;
    return rc;
}

/* ---- caller half ---- */

int sel_library_similar(sel_library* lib, const char* song_key, sel_similar_row_fn fn, void* ctx) {
    char buf[1536];
    int64_t id = 0;
    int rows = 0;

    if (lib == NULL || song_key == NULL) return -1;
    while (id < SEL_SIMILAR_LIMIT) {
        sel_genre_op op;
        const char* name;
        const char* genre;
        const char* rating;
        const char* match;

        memset(&op, 0, sizeof op);
        op.op = SEL_SIMILAR_NEXT;
        op.key = song_key;
        op.after = id;
        op.out = buf;
        op.out_cap = sizeof buf;
        if (sel_genre_call(lib, &op, &id) < 0) return -1;
        if (id == 0) break;
        name = buf + strlen(buf) + 1u;
        genre = name + strlen(name) + 1u;
        rating = genre + strlen(genre) + 1u;
        match = rating + strlen(rating) + 1u;
        if (fn != NULL) fn(ctx, buf, name, genre, atoi(rating), atoi(match));
        rows++;
    }
    return rows;
}

int sel_library_also_by(sel_library* lib, const char* song_key, sel_composer_row_fn fn, void* ctx) {
    char buf[1536];
    int64_t id = 0;
    int rows = 0;

    if (lib == NULL || song_key == NULL) return -1;
    for (;;) {
        sel_genre_op op;
        const char* name;
        const char* disp;

        memset(&op, 0, sizeof op);
        op.op = SEL_ALSOBY_NEXT;
        op.key = song_key;
        op.after = id;
        op.out = buf;
        op.out_cap = sizeof buf;
        if (sel_genre_call(lib, &op, &id) < 0) return -1;
        if (id == 0) break;
        name = buf + strlen(buf) + 1u;
        disp = name + strlen(name) + 1u;
        if (fn != NULL) fn(ctx, buf, name, disp);
        rows++;
    }
    return rows;
}
