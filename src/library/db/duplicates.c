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
    char old[33];

    if (!md5_of_file(path, hex)) return SQLITE_OK;
    sel_dup_md5_of(db, key, old);
    rc = exec2(db, "UPDATE songs SET md5 = ?2 WHERE key = ?1", key, hex);
    if (rc == SQLITE_OK) rc = exec2(db, "UPDATE files SET md5 = ?2 WHERE source = 'local' AND path = ?1", path, hex);
    if (rc == SQLITE_OK) rc = sel_dup_after(db, old, key);
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

/* SEL_DUP_PICK (I184b): next local song with id > after and no md5, without hashing it: out = key NUL path NUL. */
static int op_pick(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db,
                                "SELECT id, key, path FROM songs WHERE source = 'local' AND COALESCE(md5, '') = ''"
                                " AND id > ?1 ORDER BY id LIMIT 1",
                                -1, &st, NULL);

    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int64(st, 1, op->after);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const char* f[2];

        f[0] = (const char*)sqlite3_column_text(st, 1);
        f[1] = (const char*)sqlite3_column_text(st, 2);
        if (pack(op, f, 2)) *id = sqlite3_column_int64(st, 0);
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* SEL_DUP_STORE (I184b): md5 `value` of local song `key`, in songs and in the files row of its path. */
static int op_store(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    int rc = exec2(db,
                   "UPDATE files SET md5 = ?2 WHERE source = 'local'"
                   " AND path = (SELECT path FROM songs WHERE key = ?1 AND source = 'local')",
                   op->key, op->value);

    char old[33];

    sel_dup_md5_of(db, op->key, old);
    if (rc == SQLITE_OK) rc = exec2(db, "UPDATE songs SET md5 = ?2 WHERE key = ?1", op->key, op->value);
    if (rc == SQLITE_OK) rc = sel_dup_after(db, old, op->key);
    if (rc == SQLITE_OK) *id = 1;
    return rc;
}

/* OPEN of song `key` (I184a): the scan leaves local md5 empty, so before ranking the sources hash what the twin search
 * needs. A local song without md5 hashes itself (the file is about to be read anyway). A remote song with an md5 hashes
 * the unhashed local songs that could be its twin: same byte size when the remote row has one, else same file name. */
static int hash_for_open(sqlite3* db, const char* key) {
    sqlite3_stmt* st = NULL;
    char skey[1200];
    char spath[1200];
    char smd5[40];
    char base[sizeof spath]; /* the basename of spath, so never longer than it */
    const char* slash;
    int local;
    int64_t size;
    int64_t id = 0;
    int rc = sqlite3_prepare_v2(
        db,
        SEL_OPEN_LOOKUP_SQL,
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

/* I1094a: recompute the maintained group (songs.dup_of / dup_members) of one md5. Empty or NULL md5: nothing to group. */
int sel_dup_regroup(sqlite3* db, const char* md5) {
    sel_dup_stmts d = {0};
    int rc = sel_dup_regroup_c(db, &d, md5);

    sel_dup_stmts_close(&d);
    return rc;
}

static unsigned long g_dup_regroups;
static unsigned long g_dup_rebuilds;

unsigned long sel_db_dup_regroups(void) { return g_dup_regroups; }
unsigned long sel_db_dup_rebuilds(void) { return g_dup_rebuilds; }

/* I1094a/b: the one statement that sets group columns, for the rows the appended condition selects (one md5, or a md5 slice of the
 * rebuild). Every seek names md5 <> '' so the partial index songs_md5 serves it; without that a bound md5 = ?1 cannot prove the partial
 * index applies and each regroup scanned the whole table (catalogue-sync import 1.5 s -> 4.6..10 s on 25000 songs, I1094b). */
#define DUP_SET_SQL \
    "UPDATE songs SET dup_members = (SELECT COUNT(*) FROM songs o WHERE o.md5 = songs.md5 AND o.md5 <> '')," \
    " dup_of = NULLIF((SELECT o.key FROM songs o WHERE o.md5 = songs.md5 AND o.md5 <> '' ORDER BY " SEL_DUP_RANK_SQL("o") " LIMIT 1), songs.key)" \
    " WHERE md5 <> '' AND EXISTS (SELECT 1 FROM songs o WHERE o.md5 = songs.md5 AND o.md5 <> '' AND o.id <> songs.id) AND "

/* I1094b: prepared-once statements of the maintenance helpers. A bulk writer (scan batch) keeps one sel_dup_stmts for its whole batch, so the
 * per-row md5 read and the two regroup statements are not parsed again for every row (+12% on a 5000-row import into 20000 songs). The
 * single-row public forms below use a throwaway one. */
static const char* const DUP_MD5_SQL = "SELECT md5 FROM songs WHERE key = ?1";
static const char* const DUP_CLEAR_SQL =
    "UPDATE songs SET dup_of = NULL, dup_members = 1 WHERE md5 = ?1 AND md5 <> '' AND (dup_of IS NOT NULL OR dup_members <> 1)";

static sqlite3_stmt* dup_prep(sqlite3* db, sqlite3_stmt** slot, const char* sql) {
    if (*slot == NULL && sqlite3_prepare_v2(db, sql, -1, slot, NULL) != SQLITE_OK) *slot = NULL;
    return *slot;
}

void sel_dup_stmts_close(sel_dup_stmts* d) {
    sqlite3_finalize(d->md5);
    sqlite3_finalize(d->clear);
    sqlite3_finalize(d->set);
    memset(d, 0, sizeof *d);
}

void sel_dup_md5_of_c(sqlite3* db, sel_dup_stmts* d, const char* key, char out[33]) {
    sqlite3_stmt* st = dup_prep(db, &d->md5, DUP_MD5_SQL);

    out[0] = '\0';
    if (st == NULL) return;
    sqlite3_reset(st);
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) == SQLITE_TEXT) {
        snprintf(out, 33, "%s", (const char*)sqlite3_column_text(st, 0));
    }
    sqlite3_reset(st);
}

int sel_dup_regroup_c(sqlite3* db, sel_dup_stmts* d, const char* md5) {
    sqlite3_stmt* st[2];
    int rc = SQLITE_OK, i;

    g_dup_regroups++;
    if (md5 == NULL || md5[0] == '\0') return SQLITE_OK;
    st[0] = dup_prep(db, &d->clear, DUP_CLEAR_SQL);
    st[1] = dup_prep(db, &d->set, DUP_SET_SQL "md5 = ?1");
    for (i = 0; i < 2 && rc == SQLITE_OK; i++) {
        if (st[i] == NULL) return SQLITE_ERROR;
        sqlite3_reset(st[i]);
        sqlite3_bind_text(st[i], 1, md5, -1, SQLITE_STATIC);
        rc = sqlite3_step(st[i]);
        rc = rc == SQLITE_DONE ? SQLITE_OK : rc;
        sqlite3_reset(st[i]);
    }
    return rc;
}

int sel_dup_after_c(sqlite3* db, sel_dup_stmts* d, const char* old_md5, const char* key) {
    char now[33];
    int rc = SQLITE_OK;

    sel_dup_md5_of_c(db, d, key, now);
    if (old_md5[0] != '\0' && strcmp(old_md5, now) != 0) rc = sel_dup_regroup_c(db, d, old_md5);
    if (rc == SQLITE_OK && now[0] != '\0') rc = sel_dup_regroup_c(db, d, now);
    return rc;
}

/* I1094b: md5 of song `key`, "" when the row or the value is missing. */
void sel_dup_md5_of(sqlite3* db, const char* key, char out[33]) {
    sel_dup_stmts d = {0};

    sel_dup_md5_of_c(db, &d, key, out);
    sel_dup_stmts_close(&d);
}

/* I1094b: after a single-row md5 write (store, annotate, upsert, move, delete) regroup the old md5 and the new one. */
int sel_dup_after(sqlite3* db, const char* old_md5, const char* key) {
    sel_dup_stmts d = {0};
    int rc = sel_dup_after_c(db, &d, old_md5, key);

    sel_dup_stmts_close(&d);
    return rc;
}

/* I1094b: rows one rebuild statement may touch. One UPDATE over every grouped row held heap in proportion to the rows it changed
 * (2.2 -> 4.3 MB of the a060 tier's 5 MB heap part at 20000 songs / 1919 grouped rows inside the caller's transaction; SQLITE_NOMEM
 * from ~7500 songs on :memory:), so the rebuild walks the table in bounded slices: the heap a statement needs no longer grows with
 * the library, only the slice. */
#define DUP_SLICE_ROWS 512

static int dup_step(sqlite3_stmt* st) {
    int rc = sqlite3_step(st);

    sqlite3_reset(st);
    return rc == SQLITE_DONE || rc == SQLITE_ROW ? SQLITE_OK : rc;
}

/* I1094a/b: recompute every group; the bulk writers (and the v18 migration) call it once at the end of a bulk write, inside their
 * transaction. Pass 1 clears stale group columns of rows that are no longer in a group, by rowid slices. Pass 2 sets the groups, by
 * md5 slices of the songs_md5 index: each slice ends on a md5 at least DUP_SLICE_ROWS rows on, so a group never straddles two slices. */
int sel_dup_rebuild(sqlite3* db) {
    sqlite3_stmt *clear = NULL, *edge = NULL, *set = NULL;
    sqlite3_int64 maxid = 0, a;
    char lo[80] = "", hi[80];
    int rc;

    g_dup_rebuilds++;
    rc = sqlite3_prepare_v2(db, "SELECT MAX(id) FROM songs", -1, &edge, NULL);
    if (rc == SQLITE_OK && sqlite3_step(edge) == SQLITE_ROW) maxid = sqlite3_column_int64(edge, 0);
    sqlite3_finalize(edge);
    edge = NULL;
    /* I625a8b: no row, no group. Preparing the UPDATE statements compiles every songs trigger and opens the FTS table, which cost
     * 1657 ms of a fresh library's v18 step on a 68020 with zero rows to change. */
    if (rc != SQLITE_OK || maxid == 0) return rc;
    rc = sqlite3_prepare_v2(db,
                            "UPDATE songs SET dup_of = NULL, dup_members = 1 WHERE id > ?1 AND id <= ?2 AND (dup_of IS NOT NULL OR dup_members <> 1)"
                            " AND NOT (COALESCE(md5, '') <> '' AND EXISTS (SELECT 1 FROM songs o WHERE o.md5 = songs.md5 AND o.md5 <> '' AND o.id <> songs.id))",
                            -1, &clear, NULL);
    for (a = 0; rc == SQLITE_OK && a < maxid; a += DUP_SLICE_ROWS) {
        sqlite3_bind_int64(clear, 1, a);
        sqlite3_bind_int64(clear, 2, a + DUP_SLICE_ROWS);
        rc = dup_step(clear);
    }
    sqlite3_finalize(clear);
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(db,
                                "SELECT COALESCE((SELECT md5 FROM songs WHERE md5 > ?1 AND md5 <> '' ORDER BY md5 LIMIT 1 OFFSET ?2),"
                                " (SELECT MAX(md5) FROM songs WHERE md5 > ?1 AND md5 <> ''))",
                                -1, &edge, NULL);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(db, DUP_SET_SQL "md5 > ?1 AND md5 <= ?2", -1, &set, NULL);
    }
    while (rc == SQLITE_OK) {
        sqlite3_bind_text(edge, 1, lo, -1, SQLITE_STATIC);
        sqlite3_bind_int(edge, 2, DUP_SLICE_ROWS);
        if (sqlite3_step(edge) != SQLITE_ROW || sqlite3_column_type(edge, 0) != SQLITE_TEXT) {
            rc = sqlite3_reset(edge);
            break; /* no md5 above lo: done */
        }
        snprintf(hi, sizeof hi, "%s", (const char*)sqlite3_column_text(edge, 0));
        sqlite3_reset(edge);
        sqlite3_bind_text(set, 1, lo, -1, SQLITE_STATIC);
        sqlite3_bind_text(set, 2, hi, -1, SQLITE_STATIC);
        rc = dup_step(set);
        memcpy(lo, hi, sizeof lo);
    }
    sqlite3_finalize(edge);
    sqlite3_finalize(set);
    return rc;
}

/* Every duplicate group (same md5, two or more songs), streamed through op->row in one query: md5, member count, best key (I528). */
static int op_dup_next(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int64_t rows = 0;
    int rc = sqlite3_prepare_v2(
        db,
        "SELECT md5, COUNT(*), (SELECT s2.key FROM songs s2 WHERE s2.md5 = s.md5 ORDER BY " SEL_DUP_RANK_SQL("s2")
        " LIMIT 1) FROM songs s WHERE COALESCE(md5, '') <> '' GROUP BY md5 HAVING COUNT(*) > 1"
        " ORDER BY MIN(id)",
        -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        char count[16];
        const char* f[3];

        snprintf(count, sizeof count, "%d", sqlite3_column_int(st, 1));
        f[0] = (const char*)sqlite3_column_text(st, 0);
        f[1] = count;
        f[2] = (const char*)sqlite3_column_text(st, 2);
        rows++;
        if (op->row(op->row_ctx, f) != 0) break;
    }
    sqlite3_finalize(st);
    *id = rows;
    return rc == SQLITE_ROW || rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* The best song key among the songs sharing the md5 of song key `key`. out = best key NUL; reply id = 1 when found. */
static int op_best(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(
        db,
        SEL_BEST_SOURCE_SQL,
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
    } else if (op->op == SEL_DUP_NEXT && op->row != NULL) {
        rc = op_dup_next(lib->db, op, id);
    } else if (op->op == SEL_BEST_SOURCE && op->out != NULL && op->key != NULL) {
        rc = op_best(lib->db, op, id);
    } else if (op->op == SEL_DUP_PICK && op->out != NULL) {
        rc = op_pick(lib->db, op, id);
    } else if (op->op == SEL_DUP_STORE && op->key != NULL && op->value != NULL) {
        rc = op_store(lib->db, op, id);
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

#if !defined(SEL_LIBRARY_NO_THREAD)

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
    if (r > 0) {
        lib->hash_files++;
        lib->hash_chunks++;
    }
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

void sel_library_hash_close(sel_library* lib) {
    (void)lib;
}

void sel_library_hash_stats(sel_library* lib, sel_hash_stats* out) {
    memset(out, 0, sizeof *out);
    if (lib == NULL) return;
    pthread_mutex_lock(&lib->job_lock);
    out->files = lib->hash_files;
    out->chunks = lib->hash_chunks;
    out->tick_max = lib->hash_tick_max;
    pthread_mutex_unlock(&lib->job_lock);
}

#else

/* Without threads the pass runs on the caller's idle ticks, so one file is read in chunks of SEL_HASH_CHUNK bytes
 * across polls (I184b): a whole multi-megabyte file in one tick would stall a 68020 for seconds. */
void sel_library_hash_close(sel_library* lib) {
    if (lib != NULL && lib->hash_file != NULL) {
        fclose(lib->hash_file);
        lib->hash_file = NULL;
    }
}

void sel_library_hash_kick(sel_library* lib) {
    if (lib == NULL) return;
    sel_library_hash_close(lib); /* the pass restarts from id 0; a half-read file is picked again */
    lib->hash_after = 0;
    lib->hash_pending = 1;
}

/* Opens the next unhashed local song. 1 = opened, 0 = none left, -1 = library error, 2 = unreadable (skipped). */
static int hash_open(sel_library* lib) {
    char buf[2400];
    sel_genre_op op;
    int64_t id = 0;
    const char* path;

    memset(&op, 0, sizeof op);
    op.op = SEL_DUP_PICK;
    op.after = lib->hash_after;
    op.out = buf;
    op.out_cap = sizeof buf;
    if (sel_genre_call(lib, &op, &id) < 0) return -1;
    if (id == 0) return 0;
    lib->hash_after = id; /* an unreadable file stays unhashed, the pass goes on */
    path = buf + strlen(buf) + 1u;
    lib->hash_file = fopen(path, "rb");
    if (lib->hash_file == NULL) return 2;
    snprintf(lib->hash_key, sizeof lib->hash_key, "%s", buf);
    sel_md5_init(&lib->hash_md5);
    return 1;
}

int sel_library_hash_poll(sel_library* lib) {
    unsigned char buf[4096];
    uint32_t got = 0;
    int end = 0;

    if (lib == NULL || !lib->hash_pending) return 0;
    if (lib->hash_file == NULL) {
        int r = hash_open(lib);

        if (r <= 0) {
            lib->hash_pending = 0;
            return 0;
        }
        if (r == 2) return 1;
    }
    while (got < SEL_HASH_CHUNK) {
        size_t n = fread(buf, 1, sizeof buf, lib->hash_file);

        sel_md5_update(&lib->hash_md5, buf, n);
        got += (uint32_t)n;
        if (n < sizeof buf) {
            end = 1;
            break;
        }
    }
    lib->hash_chunks++;
    if (got > lib->hash_tick_max) lib->hash_tick_max = got;
    if (end) {
        int failed = ferror(lib->hash_file);

        sel_library_hash_close(lib);
        if (!failed) {
            char hex[33];
            sel_genre_op op;
            int64_t id = 0;

            sel_md5_hex(&lib->hash_md5, hex);
            memset(&op, 0, sizeof op);
            op.op = SEL_DUP_STORE;
            op.key = lib->hash_key;
            op.value = hex;
            if (sel_genre_call(lib, &op, &id) < 0) {
                lib->hash_pending = 0;
                return 0;
            }
            lib->hash_files++;
        }
    }
    return 1;
}

void sel_library_hash_stats(sel_library* lib, sel_hash_stats* out) {
    memset(out, 0, sizeof *out);
    if (lib == NULL) return;
    out->files = lib->hash_files;
    out->chunks = lib->hash_chunks;
    out->tick_max = lib->hash_tick_max;
}

#endif

typedef struct dup_sink {
    sel_dup_row_fn fn;
    void* ctx;
} dup_sink;

static int dup_row(void* ctx, const char* const* f) {
    dup_sink* k = ctx;

    if (k->fn != NULL) k->fn(k->ctx, f[0], atoi(f[1]), f[2]);
    return 0;
}

int sel_library_duplicates(sel_library* lib, sel_dup_row_fn fn, void* ctx) {
    dup_sink k = {fn, ctx};
    sel_genre_op op;
    int64_t rows = 0;

    if (lib == NULL || sel_library_hash_local(lib) < 0) return -1;
    memset(&op, 0, sizeof op);
    op.op = SEL_DUP_NEXT;
    op.row = dup_row;
    op.row_ctx = &k;
    if (sel_genre_call(lib, &op, &rows) < 0) return -1;
    return (int)rows;
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
