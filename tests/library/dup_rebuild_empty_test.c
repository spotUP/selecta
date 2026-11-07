/* I625a8b: sel_dup_rebuild cost 1657 ms on an EMPTY songs table at 68020 speed (I625a8 trace, v18 step). A table with no rows has no
 * group to compute, so the v18 step must not prepare or run the slice statements. Counts the statements the v18 step issues on an empty
 * table through sqlite3_trace_v2 (steps back from a full v18 database to v17 like schema_tx_test, then migrates), asserts the count is
 * at or below the number the fix leaves, that every v18 object still exists, and that a non-empty table still regroups.
 * Sentinel: sel_db_dup_rebuilds() moves during the v18 step (the rebuild ran) and "dup_rebuild_empty_ok" prints.
 * Design: thoughts/shared/plans/2026-11-06-I1094-hide-duplicates.md sections 2-3. */
#include "db/db_internal.h"
#include "db/schema.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

/* Statements the whole v18 migrate may issue on an empty table (14 before I625a8b): the user_version read and temp_store pragmas, BEGIN,
 * the 4 statements of SCHEMA_V18, the MAX(id) probe of the rebuild, the user_version write and COMMIT. No slice statement is prepared. */
#define MAX_STMTS_EMPTY 12

static int g_fail;
static int g_stmts;
static int g_print;
static int g_alter_dup;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static int on_trace(unsigned type, void* ctx, void* p, void* x) {
    (void)ctx;
    if (type == SQLITE_TRACE_STMT) {
        const char* sql = (const char*)x;

        g_stmts++;
        if (sql != NULL && strncmp(sql, "ALTER TABLE songs ADD COLUMN dup_", 33) == 0) g_alter_dup++;
        if (g_print) printf("stmt %d: %.110s\n", g_stmts, sql != NULL ? sql : "?");
    } else if (type == SQLITE_TRACE_PROFILE && g_print) {
        printf("   profile %lld ns (%s)\n", (long long)*(sqlite3_int64*)x, sqlite3_sql((sqlite3_stmt*)p) != NULL ? "stmt" : "?");
    }
    return 0;
}

static void run(sqlite3* db, const char* sql) {
    char* err = NULL;

    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        printf("FAIL sql: %s -> %s\n", sql, err != NULL ? err : "?");
        g_fail++;
        sqlite3_free(err);
    }
}

static int scalar(sqlite3* db, const char* sql) {
    sqlite3_stmt* st = NULL;
    int v = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return v;
}

int main(int argc, char** argv) {
    (void)argv;
    sqlite3* db = NULL;
    unsigned long rb0;

    g_print = argc > 1;
    CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    /* A fresh database creates the dup columns with songs: each ALTER TABLE ADD COLUMN reloads the whole schema (721 ms apiece on a
     * 68020, I625a8b rig trace), and the rebuild itself is 10 ms. */
    sqlite3_trace_v2(db, SQLITE_TRACE_STMT, on_trace, NULL);
    CHECK(sel_schema_migrate(db, NULL) != 0);
    sqlite3_trace_v2(db, 0, NULL, NULL);
    CHECK(g_alter_dup == 0);
    CHECK(sel_schema_version(db) == 18);
    run(db,
        "DROP TRIGGER IF EXISTS source_songs_state_ad; DROP TABLE source_state; DROP INDEX songs_dup_of; DROP TABLE dup_pick;"
        "ALTER TABLE songs DROP COLUMN dup_of; ALTER TABLE songs DROP COLUMN dup_members; PRAGMA user_version=17;");
    CHECK(sel_schema_version(db) == 17);

    /* v18 on an empty table. */
    sqlite3_trace_v2(db, SQLITE_TRACE_STMT | (g_print ? SQLITE_TRACE_PROFILE : 0), on_trace, NULL);
    rb0 = sel_db_dup_rebuilds();
    g_stmts = 0;
    CHECK(sel_schema_migrate(db, NULL) != 0);
    CHECK(sel_schema_version(db) == 18);
    printf("v18 on an empty table: %d statements\n", g_stmts);
    CHECK(sel_db_dup_rebuilds() == rb0 + 1); /* the rebuild ran (sentinel) */
    CHECK(g_stmts <= MAX_STMTS_EMPTY);
    sqlite3_trace_v2(db, 0, NULL, NULL);
    CHECK(scalar(db, "SELECT count(*) FROM sqlite_master WHERE name IN ('songs_dup_of', 'dup_pick')") == 2);
    CHECK(scalar(db, "SELECT count(*) FROM pragma_table_info('songs') WHERE name IN ('dup_of', 'dup_members')") == 2);

    /* A non-empty table still regroups: two equal md5 form a group. */
    run(db, "INSERT INTO songs(id, key, source, path, md5, size) VALUES(1, 'k1', 'local', 'p1', 'a', 50)");
    run(db, "INSERT INTO songs(id, key, source, path, md5, size) VALUES(2, 'k2', 'modland', 'p2', 'a', 50)");
    run(db, "INSERT INTO songs(id, key, source, path, md5, size) VALUES(3, 'k3', 'local', 'p3', 'b', 1)");
    CHECK(sel_dup_rebuild(db) == SQLITE_OK);
    CHECK(scalar(db, "SELECT dup_members FROM songs WHERE id = 1") == 2);
    CHECK(scalar(db, "SELECT dup_members FROM songs WHERE id = 2") == 2);
    CHECK(scalar(db, "SELECT dup_members FROM songs WHERE id = 3") == 1);
    CHECK(scalar(db, "SELECT dup_of IS NULL FROM songs WHERE id = 1") == 1);
    CHECK(scalar(db, "SELECT dup_of = 'k1' FROM songs WHERE id = 2") == 1);

    sqlite3_close(db);
    if (g_fail == 0) puts("dup_rebuild_empty_ok");
    return g_fail != 0;
}
