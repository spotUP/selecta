/* I848a: double-clicking a song froze the UI for 5.9 s on 644k songs. The main thread waits on SEL_BEST_SOURCE, whose twin lookup
 * ("songs with this md5") scanned songs, and whose path lookup scanned every local row. Drives sel_library_best_source (the call
 * prepare_slot makes in the real open path) on a generated catalogue and asserts: the answer is the best source of the md5 group,
 * both OPEN queries run as index SEARCHes (EXPLAIN QUERY PLAN names no SCAN of songs), Dropping songs_md5 or
 * songs_source_path makes it fail. */
#include "db.h"
#include "db_internal.h"
#include <selecta/host.h>
#include <selecta/machine.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROWS 300000
#define BATCH 50000

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static double now_ms(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

/* Plan of `sql` with ?1 bound to `arg`: 1 when no step scans songs and one step searches it through an index. */
static int plan_is_search(sqlite3* db, const char* sql, const char* arg, const char* what) {
    sqlite3_stmt* st = NULL;
    char q[1400];
    int scans = 0;
    int searches = 0;

    snprintf(q, sizeof q, "EXPLAIN QUERY PLAN %s", sql);
    if (sqlite3_prepare_v2(db, q, -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, arg, -1, SQLITE_STATIC);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* d = (const char*)sqlite3_column_text(st, 3);

        printf("plan %s: %s\n", what, d);
        if (strncmp(d, "SCAN songs", 10) == 0 || strstr(d, "(source=?)") != NULL) scans++; /* a source-only range is a scan of that source */
        if (strstr(d, "SEARCH songs USING") != NULL && strstr(d, "INDEX") != NULL) searches++;
    }
    sqlite3_finalize(st);
    return scans == 0 && searches > 0;
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    char path[512];
    char best[512];
    sel_library* lib;
    char sql[900];
    double t0, ms;
    int lo;

    snprintf(path, sizeof path, "%s/library_best_source_plan.db", argc > 1 ? argv[1] : ".");
    remove(path);
    lib = sel_library_open(host, sel_machine_fake("a060"), 0, path);
    check(lib != NULL, "open the library");
    if (lib == NULL) return 1;
    sqlite3_exec(lib->db, "DROP TRIGGER songs_ai;", NULL, NULL, NULL);
    /* Local rows (even i) and modland rows (odd i); songs with i % 20 in {0, 1} share the md5 of i / 20, the rest have none. */
    for (lo = 1; lo <= ROWS; lo += BATCH) {
        snprintf(sql, sizeof sql,
                 "WITH RECURSIVE n(i) AS (SELECT %d UNION ALL SELECT i + 1 FROM n WHERE i < %d)"
                 " INSERT INTO songs(key, source, path, name, artist, album, format, added, md5)"
                 " SELECT CASE WHEN i %% 2 = 0 THEN '/m/' || i ELSE 'modland:' || i END,"
                 " CASE WHEN i %% 2 = 0 THEN 'local' ELSE 'modland' END, '/m/' || i, 'Song ' || i, 'A', '', 'mod', 1,"
                 " CASE WHEN i %% 20 < 2 THEN 'md5-' || (i / 20) ELSE NULL END FROM n;",
                 lo, lo + BATCH - 1);
        check(sqlite3_exec(lib->db, sql, NULL, NULL, NULL) == SQLITE_OK, "fill");
    }
    /* Row 2000 is local with md5-100, row 2001 is modland with md5-100: the modland key resolves to the local path. */
    t0 = now_ms();
    best[0] = '\0';
    check(sel_library_best_source(lib, "modland:2001", best, sizeof best) == 1, "best source found");
    ms = now_ms() - t0;
    printf("best_source %.1f ms -> %s\n", ms, best);
    check(strcmp(best, "/m/2000") == 0, "the local twin wins over the modland row");
    check(plan_is_search(lib->db, SEL_BEST_SOURCE_SQL, "modland:2001", "best"), "best-source query searches songs by index");
    check(plan_is_search(lib->db, SEL_OPEN_LOOKUP_SQL, "/m/2000", "lookup"), "key/path lookup searches songs by index");
    sel_library_close(lib);
    remove(path);
    if (fails == 0) puts("library_best_source_plan_ok");
    return fails != 0;
}
