/* I1094a: the one duplicate rank (SEL_DUP_RANK_SQL) and the maintained groups (songs.dup_of / dup_members, sel_dup_regroup,
 * sel_dup_rebuild, schema v18). Design: thoughts/shared/plans/2026-11-06-I1094-hide-duplicates.md sections 2-3.
 * Fixture, three groups plus loners:
 *   a: local(id1) + modland(id2)            -> best local; after a user pick of the modland copy -> modland
 *   b: modland 10 bytes(id3) + modland 20 bytes(id4) + hvsc 99 bytes(id5) -> source rank beats size (modland), size beats id (id4)
 *   c: hvsc(id6) + hvsc(id7), equal size    -> lowest id (id6)
 * plus a unique md5 song and two songs without md5 (never grouped). dup_regroups / dup_rebuilds are the sentinels that the code ran. */
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include "db/db_internal.h"
#include "db/schema.h"

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void run(sqlite3* db, const char* sql) {
    char* err = NULL;

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        printf("FAIL sql: %s -> %s\n", sql, err != NULL ? err : "?");
        g_fail++;
        sqlite3_free(err);
    }
}

static void song(sqlite3* db, int id, const char* source, const char* md5, int size) {
    char sql[256];

    snprintf(sql, sizeof sql, "INSERT INTO songs(id, key, source, path, md5, size) VALUES(%d, 'k%d', '%s', 'p%d', %s%s%s, %d)", id, id,
             source, id, md5 != NULL ? "'" : "", md5 != NULL ? md5 : "NULL", md5 != NULL ? "'" : "", size);
    run(db, sql);
}

/* dup_of of song k<id> as text ("-" for NULL) and its dup_members. */
static void state(sqlite3* db, int id, char of[16], int* members) {
    sqlite3_stmt* st = NULL;
    char sql[96];

    snprintf(sql, sizeof sql, "SELECT dup_of, dup_members FROM songs WHERE id = %d", id);
    snprintf(of, 16, "?");
    *members = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) {
        const char* t = (const char*)sqlite3_column_text(st, 0);

        snprintf(of, 16, "%s", t != NULL ? t : "-");
        *members = sqlite3_column_int(st, 1);
    }
    sqlite3_finalize(st);
}

static int is(sqlite3* db, int id, const char* of, int members) {
    char got[16];
    int m;

    state(db, id, got, &m);
    if (strcmp(got, of) != 0 || m != members) printf("  k%d: dup_of=%s members=%d, want %s %d\n", id, got, m, of, members);
    return strcmp(got, of) == 0 && m == members;
}

static void best_of(sqlite3* db, int id, char out[16]) {
    sqlite3_stmt* st = NULL;

    snprintf(out, 16, "?");
    if (sqlite3_prepare_v2(db, SEL_BEST_SOURCE_SQL, -1, &st, NULL) == SQLITE_OK) {
        char key[16];

        snprintf(key, sizeof key, "k%d", id);
        sqlite3_bind_text(st, 1, key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) snprintf(out, 16, "%s", (const char*)sqlite3_column_text(st, 0));
    }
    sqlite3_finalize(st);
}

int main(void) {
    sqlite3* db = NULL;
    char best[16];
    unsigned long rg0, rb0;

    CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    CHECK(sel_schema_migrate(db, NULL) != 0);
    CHECK(sel_schema_version(db) == 19);

    song(db, 1, "local", "a", 50);
    song(db, 2, "modland", "a", 50);
    song(db, 3, "modland", "b", 10);
    song(db, 4, "modland", "b", 20);
    song(db, 5, "hvsc", "b", 99);
    song(db, 6, "hvsc", "c", 7);
    song(db, 7, "hvsc", "c", 7);
    song(db, 8, "local", "d", 1);
    song(db, 9, "local", NULL, 1);
    song(db, 10, "local", "", 1);

    /* The bulk path: one rebuild resolves every group. */
    rb0 = sel_db_dup_rebuilds();
    CHECK(sel_dup_rebuild(db) == SQLITE_OK);
    CHECK(sel_db_dup_rebuilds() == rb0 + 1);
    CHECK(is(db, 1, "-", 2));
    CHECK(is(db, 2, "k1", 2));
    CHECK(is(db, 3, "k4", 3)); /* source rank beats the bigger hvsc copy; size beats the lower id */
    CHECK(is(db, 4, "-", 3));
    CHECK(is(db, 5, "k4", 3));
    CHECK(is(db, 6, "-", 2)); /* equal everything: lowest id */
    CHECK(is(db, 7, "k6", 2));
    CHECK(is(db, 8, "-", 1));
    CHECK(is(db, 9, "-", 1));
    CHECK(is(db, 10, "-", 1));

    /* OPEN's best-source query and the maintained column agree. */
    best_of(db, 2, best);
    CHECK(strcmp(best, "k1") == 0);
    best_of(db, 5, best);
    CHECK(strcmp(best, "k4") == 0);
    best_of(db, 7, best);
    CHECK(strcmp(best, "k6") == 0);

    /* The user's pick outranks the source rank: group a now prefers the modland copy, both readers agree. */
    run(db, "INSERT INTO dup_pick(md5, key) VALUES('a', 'k2')");
    rg0 = sel_db_dup_regroups();
    CHECK(sel_dup_regroup(db, "a") == SQLITE_OK);
    CHECK(sel_db_dup_regroups() == rg0 + 1);
    CHECK(is(db, 1, "k2", 2));
    CHECK(is(db, 2, "-", 2));
    CHECK(is(db, 4, "-", 3)); /* other groups untouched */
    best_of(db, 1, best);
    CHECK(strcmp(best, "k2") == 0);

    /* A song leaves group b and one joins group d: regroup both md5 values (the single-row writers do this). */
    run(db, "DELETE FROM songs WHERE id = 4");
    CHECK(sel_dup_regroup(db, "b") == SQLITE_OK);
    CHECK(is(db, 3, "-", 2)); /* the 10 byte modland copy is now the best */
    CHECK(is(db, 5, "k3", 2));
    run(db, "UPDATE songs SET md5 = 'd' WHERE id = 6");
    CHECK(sel_dup_regroup(db, "c") == SQLITE_OK);
    CHECK(sel_dup_regroup(db, "d") == SQLITE_OK);
    CHECK(is(db, 7, "-", 1)); /* c is down to one member: no longer a duplicate */
    CHECK(is(db, 6, "k8", 2)); /* local k8 beats hvsc k6 */
    CHECK(is(db, 8, "-", 2));

    /* The regroup result equals a fresh rebuild. */
    {
        char o[8][16];
        int i, m, same = 1;

        for (i = 1; i <= 8; i++) state(db, i, o[i - 1], &m);
        CHECK(sel_dup_rebuild(db) == SQLITE_OK);
        for (i = 1; i <= 8; i++) {
            char g[16];

            state(db, i, g, &m);
            if (i != 4 && strcmp(g, o[i - 1]) != 0) same = 0;
        }
        CHECK(same);
    }

    sqlite3_close(db);
    if (g_fail != 0) {
        printf("dup_rank_fail %d\n", g_fail);
        return 1;
    }
    printf("dup_rank_ok\n");
    return 0;
}
