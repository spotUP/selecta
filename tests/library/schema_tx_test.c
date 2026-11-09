/* I625a8: a fresh library.db (user_version 0) is created in ONE transaction. Eighteen BEGIN/COMMIT pairs meant eighteen journal
 * create/sync/delete cycles, 7.7 s of the cold start on a 68020 (I625a5). An existing database still migrates one version per
 * transaction, so a half-applied upgrade rolls back alone. Driven through sel_schema_migrate (the call sel_library_open makes);
 * sentinel: user_version reaches SEL_LIB_SCHEMA_VERSION and "schema_tx_ok" prints. Counts BEGIN / COMMIT / ROLLBACK statements
 * through sqlite3_trace_v2 and commits through the commit hook. No timing. */
#include "db.h"
#include "schema.h"
#include <selecta/host.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static int fails;
static int n_begin, n_commit, n_rollback, n_hook;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static int on_stmt(unsigned type, void* ctx, void* p, void* x) {
    const char* sql = (const char*)x;

    (void)ctx;
    (void)p;
    if (type == SQLITE_TRACE_STMT && sql != NULL) {
        n_begin += strncmp(sql, "BEGIN", 5) == 0;
        n_commit += strncmp(sql, "COMMIT", 6) == 0;
        n_rollback += strncmp(sql, "ROLLBACK", 8) == 0;
    }
    return 0;
}

static int on_commit(void* ctx) {
    (void)ctx;
    n_hook++;
    return 0;
}

static void reset(sqlite3* db) {
    n_begin = n_commit = n_rollback = n_hook = 0;
    sqlite3_trace_v2(db, SQLITE_TRACE_STMT, on_stmt, NULL);
    sqlite3_commit_hook(db, on_commit, NULL);
}

static int scalar(sqlite3* db, const char* sql) {
    sqlite3_stmt* st = NULL;
    int v = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) {
        v = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return v;
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    char path[512];
    sqlite3* db = NULL;

    snprintf(path, sizeof path, "%s/schema_tx_test.db", argc > 1 ? argv[1] : ".");
    remove(path);
    check(sqlite3_open(path, &db) == SQLITE_OK, "open a new file");

    /* Fresh: one transaction. */
    reset(db);
    check(sel_schema_migrate(db, host) == 1, "fresh migrate succeeds");
    check(sel_schema_version(db) == SEL_LIB_SCHEMA_VERSION, "fresh: user_version is current");
    printf("fresh: begin=%d commit=%d rollback=%d hook=%d\n", n_begin, n_commit, n_rollback, n_hook);
    check(n_begin == 1, "fresh: exactly one BEGIN");
    check(n_commit == 1 && n_hook == 1, "fresh: exactly one COMMIT");
    check(n_rollback == 0, "fresh: no ROLLBACK");
    check(scalar(db, "SELECT count(*) FROM sqlite_master WHERE name IN ('songs_dup_of','dup_pick','source_state','songs_fts')") >= 3,
          "fresh: v1 to v19 objects exist");

    /* Existing v16: v17, v18 and v19 still run as one transaction each (v19 search_cache is created IF NOT EXISTS). */
    check(sqlite3_exec(db,
                       "DROP TRIGGER source_songs_state_ad; DROP TABLE source_state;"
                       "DROP INDEX songs_dup_of; DROP TABLE dup_pick; ALTER TABLE songs DROP COLUMN dup_of;"
                       "ALTER TABLE songs DROP COLUMN dup_members; PRAGMA user_version=16;",
                       NULL, NULL, NULL) == SQLITE_OK,
          "step back to v16");
    reset(db);
    check(sel_schema_migrate(db, host) == 1, "v16 migrate succeeds");
    check(sel_schema_version(db) == SEL_LIB_SCHEMA_VERSION, "v16: user_version is current");
    printf("v16: begin=%d commit=%d rollback=%d hook=%d\n", n_begin, n_commit, n_rollback, n_hook);
    check(n_begin == 3 && n_commit == 3 && n_hook == 3, "v16: one transaction per missing version (3)");
    check(n_rollback == 0, "v16: no ROLLBACK");

    sqlite3_close(db);
    remove(path);
    if (fails == 0) {
        puts("schema_tx_ok");
    }
    return fails != 0;
}
