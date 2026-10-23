/* Duplicates by md5 across sources and the best source of a song (plan 2026-10-25-m3-library.md I184, T86;
 * decision D-M3-14). Worker half of SEL_DUP_HASH / SEL_DUP_NEXT / SEL_BEST_SOURCE and the caller halves
 * sel_library_hash_local / sel_library_duplicates / sel_library_best_source.
 *
 * A duplicate group is a set of two or more songs (different keys) with the same non-empty md5. The scan leaves
 * the md5 of local files empty (it only stats), so the duplicates step hashes them first (SEL_DUP_HASH, once per
 * file: the value is stored in songs.md5 and files.md5). Best source, default of D-M3-14: local > home mirror >
 * cache > modland > hvsc > hippo URL. The songs table knows the sources local, modland, hvsc and hippo; the home
 * mirror and the cache are transports of a modland/hvsc key, not sources of a row. Ties (same source) go to the
 * song added first. */
#include "db_internal.h"
#include "../md5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Rank of a source in the D-M3-14 order, lower is better. */
#define SEL_SOURCE_RANK_SQL \
    "CASE source WHEN 'local' THEN 0 WHEN 'modland' THEN 3 WHEN 'hvsc' THEN 4 WHEN 'hippo' THEN 5 ELSE 6 END"

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

static int md5_of_file(const char* path, char hex[33]) {
    FILE* f = fopen(path, "rb");
    sel_md5 c;
    unsigned char buf[4096];
    size_t n;

    if (f == NULL) return 0;
    sel_md5_init(&c);
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) sel_md5_update(&c, buf, n);
    fclose(f);
    sel_md5_hex(&c, hex);
    return 1;
}

static int exec2(sqlite3* db, const char* sql, const char* a, const char* b) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);

    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, a, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, b, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* Next local song with id > after and no md5: hashes the file and stores the value. Reply id = that song's id. */
static int op_hash(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    char hex[33];
    char key[1200];
    char path[1200];
    int rc = sqlite3_prepare_v2(
        db, "SELECT id, key, path FROM songs WHERE source = 'local' AND COALESCE(md5, '') = '' AND id > ?1 ORDER BY id LIMIT 1",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int64(st, 1, op->after);
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(st);
        return rc == SQLITE_DONE ? SQLITE_OK : rc;
    }
    *id = sqlite3_column_int64(st, 0);
    snprintf(key, sizeof key, "%s", (const char*)sqlite3_column_text(st, 1));
    snprintf(path, sizeof path, "%s", (const char*)sqlite3_column_text(st, 2));
    sqlite3_finalize(st);
    if (!md5_of_file(path, hex)) return SQLITE_OK; /* unreadable: stays unhashed, the scan goes on */
    rc = exec2(db, "UPDATE songs SET md5 = ?2 WHERE key = ?1", key, hex);
    if (rc == SQLITE_OK) rc = exec2(db, "UPDATE files SET md5 = ?2 WHERE source = 'local' AND path = ?1", path, hex);
    return rc;
}

/* The group at row offset `after`: out = md5 NUL member count NUL best key NUL. */
static int op_dup_next(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT md5, COUNT(*), (SELECT s2.key FROM songs s2 WHERE s2.md5 = s.md5 ORDER BY " SEL_SOURCE_RANK_SQL
        ", s2.id LIMIT 1) FROM songs s WHERE COALESCE(md5, '') <> '' GROUP BY md5 HAVING COUNT(*) > 1"
        " ORDER BY MIN(id) LIMIT 1 OFFSET ?1",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int64(st, 1, op->after);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        char count[16];
        const char* f[3];

        snprintf(count, sizeof count, "%d", sqlite3_column_int(st, 1));
        f[0] = (const char*)sqlite3_column_text(st, 0);
        f[1] = count;
        f[2] = (const char*)sqlite3_column_text(st, 2);
        if (pack(op, f, 3)) *id = op->after + 1;
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* The best song key among the songs sharing the md5 of song key `key`. out = best key NUL; reply id = 1 when found. */
static int op_best(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT key FROM songs WHERE COALESCE(md5, '') <> '' AND md5 = (SELECT md5 FROM songs WHERE key = ?1)"
        " ORDER BY " SEL_SOURCE_RANK_SQL ", id LIMIT 1",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const char* f[1];

        f[0] = (const char*)sqlite3_column_text(st, 0);
        if (pack(op, f, 1)) *id = 1;
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int sel_library_duplicates_op(sel_library* lib, const sel_genre_op* op, int* found, int64_t* id) {
    int rc = SQLITE_MISUSE;

    *found = 0;
    *id = 0;
    if (op->op == SEL_DUP_HASH) {
        rc = op_hash(lib->db, op, id);
    } else if (op->op == SEL_DUP_NEXT && op->out != NULL) {
        rc = op_dup_next(lib->db, op, id);
    } else if (op->op == SEL_BEST_SOURCE && op->out != NULL && op->key != NULL) {
        rc = op_best(lib->db, op, id);
    }
    *found = *id != 0;
    return rc;
}

/* ---- caller half ---- */

int sel_library_hash_local(sel_library* lib) {
    int64_t id = 0;
    int n = 0;

    if (lib == NULL) return -1;
    for (;;) {
        sel_genre_op op;

        memset(&op, 0, sizeof op);
        op.op = SEL_DUP_HASH;
        op.after = id;
        if (sel_genre_call(lib, &op, &id) < 0) return -1;
        if (id == 0) break;
        n++;
    }
    return n;
}

int sel_library_duplicates(sel_library* lib, sel_dup_row_fn fn, void* ctx) {
    char buf[1536];
    int64_t id = 0;
    int rows = 0;

    if (lib == NULL || sel_library_hash_local(lib) < 0) return -1;
    for (;;) {
        sel_genre_op op;
        const char* count;
        const char* best;

        memset(&op, 0, sizeof op);
        op.op = SEL_DUP_NEXT;
        op.after = id;
        op.out = buf;
        op.out_cap = sizeof buf;
        if (sel_genre_call(lib, &op, &id) < 0) return -1;
        if (id == 0) break;
        count = buf + strlen(buf) + 1u;
        best = count + strlen(count) + 1u;
        if (fn != NULL) fn(ctx, buf, atoi(count), best);
        rows++;
    }
    return rows;
}

int sel_library_best_source(sel_library* lib, const char* song_key, char* out, size_t cap) {
    char buf[1536];
    sel_genre_op op;
    int64_t id = 0;

    if (lib == NULL || song_key == NULL || out == NULL || cap == 0) return 0;
    memset(&op, 0, sizeof op);
    op.op = SEL_BEST_SOURCE;
    op.key = song_key;
    op.out = buf;
    op.out_cap = sizeof buf;
    if (sel_genre_call(lib, &op, &id) <= 0 || strlen(buf) + 1u > cap) return 0;
    memcpy(out, buf, strlen(buf) + 1u);
    return 1;
}
