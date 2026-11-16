/* I1420: opening a composer/artist page ran one query per song, each scanning the whole songs table (5.6 s for 532 songs on 672k rows).
 * Checks: the page lists exactly the composer's songs, the unknown group lists the songs without a composer, and the named query
 * is driven by the song_composers_canon index (its plan has no SCAN of songs). */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "composer.h"
#include "db.h"
#include "db_internal.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void row(void* ctx, const char* key, const char* name, const char* disp) {
    (void)key;
    (void)name;
    (void)disp;
    ++*(int*)ctx;
}

static int plan_scans_songs(sqlite3* db) {
    sqlite3_stmt* st = NULL;
    char sql[1200];
    int scan = 0;

    snprintf(sql, sizeof sql, "EXPLAIN QUERY PLAN %s", SEL_COMPOSER_NEXT_SQL);
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, "zed", -1, SQLITE_STATIC);
    sqlite3_bind_int64(st, 2, 0);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* d = (const char*)sqlite3_column_text(st, 3);

        if (d != NULL && strstr(d, "SCAN songs") != NULL) scan = 1;
    }
    sqlite3_finalize(st);
    return scan;
}

int main(void) {
    sel_library* lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, ":memory:");
    char sql[300];
    int i, named = 0, unknown = 0;

    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    for (i = 0; i < 300; i++) {
        snprintf(sql, sizeof sql, "INSERT INTO songs(key, source, path, name, format, added) VALUES('k%d', 'local', '/m/%d.mod', 'S%d', 'mod', 1)", i, i, i);
        check(sqlite3_exec(lib->db, sql, NULL, NULL, NULL) == SQLITE_OK, "insert song");
        if (i % 60 == 0) {
            snprintf(sql, sizeof sql, "INSERT INTO song_composers(song_key, canon, display) VALUES('k%d', 'zed', 'Zed')", i);
            check(sqlite3_exec(lib->db, sql, NULL, NULL, NULL) == SQLITE_OK, "insert composer");
        }
    }
    check(sel_library_composer_page(lib, "Zed", row, &named) == 5 && named == 5, "named composer lists its 5 songs");
    check(sel_library_composer_page(lib, SEL_COMPOSER_UNKNOWN_CANON, row, &unknown) == 295 && unknown == 295, "unknown group lists the other 295");
    check(plan_scans_songs(lib->db) == 0, "named query is driven by the canon index, no SCAN of songs");
    sel_library_close(lib);
    if (fails == 0) printf("composer_page_ok\n");
    return fails != 0;
}
