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
#include "../source.h"

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

/* Hashes the file at `path` and stores the value for song `key` (songs and files tables). An unreadable file stays
 * unhashed. */
static int store_hash(sqlite3* db, const char* key, const char* path) {
    char hex[33];
    int rc;

    if (!md5_of_file(path, hex)) return SQLITE_OK;
    rc = exec2(db, "UPDATE songs SET md5 = ?2 WHERE key = ?1", key, hex);
    if (rc == SQLITE_OK) rc = exec2(db, "UPDATE files SET md5 = ?2 WHERE source = 'local' AND path = ?1", path, hex);
    return rc;
}

/* Next local song with id > after and no md5, optionally narrowed to the songs of byte size `size` (> 0) or, when
 * `size` <= 0 and `base` is set, to the songs whose path ends in `base`: hashes the file and stores the value.
 * *id = that song's id (0: none left). */
static int hash_next(sqlite3* db, int64_t after, int64_t size, const char* base, int64_t* id) {
    sqlite3_stmt* st = NULL;
    char key[1200];
    char path[1200];
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT id, key, path FROM songs WHERE source = 'local' AND COALESCE(md5, '') = '' AND id > ?1"
        " AND (?2 <= 0 OR size = ?2) AND (?2 > 0 OR ?3 IS NULL OR substr(path, -length(?3)) = ?3) ORDER BY id LIMIT 1",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int64(st, 1, after);
    sqlite3_bind_int64(st, 2, size);
    if (base != NULL) {
        sqlite3_bind_text(st, 3, base, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(st, 3);
    }
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(st);
        return rc == SQLITE_DONE ? SQLITE_OK : rc;
    }
    *id = sqlite3_column_int64(st, 0);
    snprintf(key, sizeof key, "%s", (const char*)sqlite3_column_text(st, 1));
    snprintf(path, sizeof path, "%s", (const char*)sqlite3_column_text(st, 2));
    sqlite3_finalize(st);
    return store_hash(db, key, path); /* unreadable: stays unhashed, the pass goes on */
}

/* Next local song with id > after and no md5: hashes the file and stores the value. Reply id = that song's id. */
static int op_hash(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    return hash_next(db, op->after, 0, NULL, id);
}

/* OPEN of song `key` (I184a): the scan leaves local md5 empty, so before ranking the sources hash what the twin search
 * needs. A local song without md5 hashes itself (the file is about to be read anyway). A remote song with an md5 hashes
 * the unhashed local songs that could be its twin: same byte size when the remote row has one, else same file name. */
static int hash_for_open(sqlite3* db, const char* key) {
    sqlite3_stmt* st = NULL;
    char skey[1200];
    char spath[1200];
    char smd5[40];
    char base[300];
    const char* slash;
    int local;
    int64_t size;
    int64_t id = 0;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT key, path, source = 'local', COALESCE(md5, ''), COALESCE(size, 0) FROM songs"
        " WHERE key = ?1 OR (source = 'local' AND path = ?1) LIMIT 1",
        -1, &st, NULL);

    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(st);
        return rc == SQLITE_DONE ? SQLITE_OK : rc;
    }
    snprintf(skey, sizeof skey, "%s", (const char*)sqlite3_column_text(st, 0));
    snprintf(spath, sizeof spath, "%s", (const char*)sqlite3_column_text(st, 1));
    local = sqlite3_column_int(st, 2);
    snprintf(smd5, sizeof smd5, "%s", (const char*)sqlite3_column_text(st, 3));
    size = sqlite3_column_int64(st, 4);
    sqlite3_finalize(st);
    if (local) return smd5[0] == '\0' ? store_hash(db, skey, spath) : SQLITE_OK;
    if (smd5[0] == '\0') return SQLITE_OK;
    slash = strrchr(spath, '/');
    snprintf(base, sizeof base, "%s", slash != NULL ? slash + 1 : spath);
    if (size <= 0 && base[0] == '\0') return SQLITE_OK; /* no narrowing key: the background pass covers it */
    for (;;) {
        rc = hash_next(db, id, size, base, &id);
        if (rc != SQLITE_OK || id == 0) return rc;
    }
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
    rc = hash_for_open(db, op->key);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(st);
        return rc;
    }
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

/* One step of the idle pass: the next unhashed local song after *after. 1 = hashed one, 0 = none left, -1 = error. */
static int hash_step(sel_library* lib, int64_t* after) {
    sel_genre_op op;
    int64_t id = 0;

    memset(&op, 0, sizeof op);
    op.op = SEL_DUP_HASH;
    op.after = *after;
    if (sel_genre_call(lib, &op, &id) < 0) return -1;
    if (id == 0) return 0;
    *after = id;
    return 1;
}

#if !defined(SEL_LIBRARY_NO_THREAD)

void sel_hash_job_run(sel_library* lib) {
    int64_t after;
    int r;

    if (lib->hash_pending) {
        lib->hash_pending = 0;
        lib->hash_after = 0;
    }
    after = lib->hash_after;
    pthread_mutex_unlock(&lib->job_lock);
    r = hash_step(lib, &after);
    pthread_mutex_lock(&lib->job_lock);
    if (!lib->hash_pending) lib->hash_after = after; /* a kick during the file restarts the pass instead */
    if (r > 0 || lib->hash_pending) lib->job_hash = 1; /* one file per turn: ratings, votes and fetches go first */
}

void sel_library_hash_kick(sel_library* lib) {
    if (lib == NULL || !sel_job_start(lib)) return;
    pthread_mutex_lock(&lib->job_lock);
    lib->hash_pending = 1;
    lib->job_hash = 1;
    sel_job_wake(lib);
    pthread_mutex_unlock(&lib->job_lock);
}

int sel_library_hash_poll(sel_library* lib) {
    (void)lib;
    return 0;
}

#else

void sel_library_hash_kick(sel_library* lib) {
    if (lib == NULL) return;
    lib->hash_after = 0;
    lib->hash_pending = 1;
}

int sel_library_hash_poll(sel_library* lib) {
    int r;

    if (lib == NULL || !lib->hash_pending) return 0;
    r = hash_step(lib, &lib->hash_after);
    if (r <= 0) lib->hash_pending = 0;
    return r > 0;
}

#endif

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
    char source[32];
    char path[1536];
    sel_genre_op op;
    int64_t id = 0;

    if (lib == NULL || song_key == NULL || out == NULL || cap == 0) return 0;
    memset(&op, 0, sizeof op);
    op.op = SEL_BEST_SOURCE;
    op.key = song_key;
    op.out = buf;
    op.out_cap = sizeof buf;
    if (sel_genre_call(lib, &op, &id) <= 0) return 0;
    /* A local key opens as its plain path; a remote key stays a key for the file cache. */
    if (sel_song_key_split(buf, source, sizeof source, path, sizeof path) && strcmp(source, "local") == 0) {
        memmove(buf, path, strlen(path) + 1u);
    }
    if (strlen(buf) + 1u > cap) return 0;
    memcpy(out, buf, strlen(buf) + 1u);
    return 1;
}
