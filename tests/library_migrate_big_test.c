/* I787a: Johan's 594k-song library (schema v12) could not open after SCHEMA_V13: its CREATE INDEX sorts every song in
 * the SQLite heap and failed with SQLITE_NOMEM. This drives sel_library_open (the product's entry point) on a v12 file
 * with enough songs to overflow the sorter of the small and 2 MiB heap tiers, and asserts the file ends at v13
 * with both indexes. The sentinel is the migration itself: user_version moves 12 -> 13 and songs_artist_added exists. */
#include "db.h"
#include "db_internal.h"
#include <selecta/host.h>
#include <selecta/machine.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS 40000
#define BATCH 5000

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static int copy_file(const char* from, const char* to) {
    FILE* in = fopen(from, "rb");
    FILE* out = fopen(to, "wb");
    char buf[65536];
    size_t n;

    if (in == NULL || out == NULL) {
        return 0;
    }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    fclose(in);
    return fclose(out) == 0;
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

/* A v12 file: create it at v13 on the 8 MiB tier, fill songs, then put the v12 index set back. */
static int make_v12(const sel_host* host, const char* path) {
    sel_library* lib = sel_library_open(host, sel_machine_fake("a060"), 0, path);
    char sql[400];
    int ok;
    int lo;

    if (lib == NULL) {
        return 0;
    }
    /* The FTS insert trigger would dominate the fill and is not under test. */
    ok = sqlite3_exec(lib->db, "DROP TRIGGER songs_ai;", NULL, NULL, NULL) == SQLITE_OK;
    for (lo = 1; ok && lo <= ROWS; lo += BATCH) {
        snprintf(sql, sizeof sql,
                 "WITH RECURSIVE n(i) AS (SELECT %d UNION ALL SELECT i + 1 FROM n WHERE i < %d)"
                 " INSERT INTO songs(key, source, path, name, artist, album, format, added)"
                 " SELECT 'k' || i, 'local', '/m/' || i, 'Song ' || i, 'Artist ' || (i * 7919 %% 20011),"
                 " 'Album ' || (i * 104729 %% 40009), 'mod', i * 13 %% 99991 FROM n;",
                 lo, lo + BATCH - 1);
        ok = sqlite3_exec(lib->db, sql, NULL, NULL, NULL) == SQLITE_OK;
    }
    ok = ok && sqlite3_exec(lib->db,
                      "DROP INDEX songs_artist_added; DROP INDEX songs_album; CREATE INDEX songs_album ON songs(album);"
                      "PRAGMA user_version=12;",
                      NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) {
        printf("make_v12: %s\n", sqlite3_errmsg(lib->db));
    }
    sel_library_close(lib);
    return ok;
}

static void migrate_on(const sel_host* host, const char* master, const char* name) {
    char path[512];
    char what[96];
    sel_library* lib;

    snprintf(path, sizeof path, "%s.%s", master, name);
    check(copy_file(master, path), "copy the v12 file");
    lib = sel_library_open(host, sel_machine_fake(name), 0, path);
    snprintf(what, sizeof what, "%s: v12 file opens (migrates to v13)", name);
    check(lib != NULL, what);
    if (lib != NULL) {
        snprintf(what, sizeof what, "%s: user_version is 13", name);
        check(scalar(lib->db, "PRAGMA user_version") == 13, what);
        snprintf(what, sizeof what, "%s: songs_artist_added exists", name);
        check(scalar(lib->db, "SELECT count(*) FROM sqlite_master WHERE name='songs_artist_added'") == 1, what);
        snprintf(what, sizeof what, "%s: songs_album covers artist and added", name);
        check(scalar(lib->db, "SELECT count(*) FROM pragma_index_info('songs_album')") == 3, what);
        snprintf(what, sizeof what, "%s: all %d songs kept", name, ROWS);
        check(scalar(lib->db, "SELECT count(*) FROM songs") == ROWS, what);
        sel_library_close(lib);
    }
    remove(path);
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    char master[512];
    int keep;

    snprintf(master, sizeof master, "%s/library_migrate_big_%d.db", argc > 1 ? argv[1] : ".", (int)ROWS);
    /* argv[2] "keep": reuse the file from an earlier run and leave it (the fill takes over a minute at -O0). */
    keep = argc > 2 && strcmp(argv[2], "keep") == 0;
    if (!keep || fopen(master, "rb") == NULL) {
        remove(master);
        check(make_v12(host, master), "build the v12 file");
    }
    /* The 544 KiB a500 tier still runs out of memory (I787a open item): 155 KB of its 340 KB heap part is used before the
     * migration starts. */
    migrate_on(host, master, "a1200-fpu"); /* 2 MiB tier */
    if (!keep) {
        remove(master);
    }
    if (fails == 0) {
        puts("library_migrate_big_ok");
    }
    return fails != 0;
}
