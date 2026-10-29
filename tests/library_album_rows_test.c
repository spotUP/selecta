/* I687: every distinct (album, artist, source) of songs with a non-empty album has one albums row, so an album without an
 * embedded cover reaches the Library Albums list (PART_ALBUM lists FROM albums). Drives sel_library_items (the Your Library list
 * query) after plain song inserts; the cover store (SEL_ART_STORE, what the scan sends) must reuse the trigger's row; the last
 * song's delete removes the row; and an old v9 database gets its albums backfilled by the migration. */
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void sql(sqlite3* db, const char* text) {
    char* err = NULL;

    if (sqlite3_exec(db, text, NULL, NULL, &err) != SQLITE_OK) {
        printf("FAIL sql: %s (%s)\n", text, err != NULL ? err : "?");
        fails++;
    }
}

static int64_t scalar(sqlite3* db, const char* text) {
    sqlite3_stmt* st = NULL;
    int64_t v = -1;

    if (sqlite3_prepare_v2(db, text, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return v;
}

/* Rows of the Albums list named `name`, and the songs count of the first. */
static int listed(sel_library* lib, const char* name, unsigned* songs) {
    static sel_library_item rows[64];
    uint32_t total = 0, n, i;
    int found = 0;

    n = sel_library_items(lib, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM), 0, rows, 64u, &total);
    for (i = 0; i < n; i++) {
        if (strcmp(rows[i].name, name) == 0) {
            if (found == 0 && songs != NULL) *songs = rows[i].songs;
            found++;
        }
    }
    return found;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_albumrows_XXXXXX";
    char path[256];
    sel_library* lib;
    sqlite3* db;
    unsigned songs = 0;
    sel_art_op op;
    sel_message reply;
    static const uint8_t img[4] = {1, 2, 3, 4};

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(path, sizeof path, "%s/lib.db", dir);
    lib = sel_library_open(host, sel_machine_probe(host), 0, path);
    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    db = lib->db;

    /* A tagged song with no cover: one albums row, listed with its song. */
    sql(db, "INSERT INTO songs(key, source, path, name, artist, album, added) VALUES"
            "('local:a1','local','a1','one','Ant Farm','ANThology',10),('local:a2','local','a2','two','Ant Farm','ANThology',20),"
            "('local:n1','local','n1','none','Ant Farm',NULL,30),('local:e1','local','e1','empty','Ant Farm','',40)");
    check(scalar(db, "SELECT COUNT(*) FROM albums WHERE name='ANThology' AND artist='Ant Farm' AND source='local'") == 1, "one row for two songs");
    check(scalar(db, "SELECT COUNT(*) FROM albums") == 1, "no row for NULL or empty album");
    check(listed(lib, "ANThology", &songs) == 1 && songs == 2u, "album listed once with 2 songs");

    /* A cover store (what the scan sends) finds the trigger's row: still one row, now with artwork. */
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_STORE;
    op.name = "ANThology";
    op.artist = "Ant Farm";
    op.kind = "cover";
    op.data = (uint8_t*)img;
    op.len = sizeof img;
    sel_library_art_op(lib, &op, &reply);
    check(reply.args[0].v.i == 1, "cover stored");
    check(scalar(db, "SELECT COUNT(*) FROM albums") == 1, "cover store adds no duplicate");
    check(scalar(db, "SELECT COUNT(*) FROM artwork") == 1, "artwork row");

    /* Cover first, then the song: still one row. */
    op.name = "Covered";
    op.artist = "Zed";
    sel_library_art_op(lib, &op, &reply);
    sql(db, "INSERT INTO songs(key, source, path, name, artist, album) VALUES('local:c1','local','c1','c','Zed','Covered')");
    check(scalar(db, "SELECT COUNT(*) FROM albums WHERE name='Covered'") == 1, "song after cover: one row");

    /* Retagging adds the new album (the emptied old one stays: orphan cleanup in the update trigger blows the 512 KB scan heap, I577). */
    sql(db, "UPDATE songs SET album='Retagged' WHERE key='local:c1'");
    check(scalar(db, "SELECT COUNT(*) FROM albums WHERE name='Retagged'") == 1, "retag adds the new album");

    /* Deleting one of two songs keeps the row; the last one removes it. */
    sql(db, "DELETE FROM songs WHERE key='local:a1'");
    check(listed(lib, "ANThology", &songs) == 1 && songs == 1u, "one song left, album stays");
    sql(db, "DELETE FROM songs WHERE key='local:a2'");
    check(listed(lib, "ANThology", NULL) == 0, "last song gone: album not listed");
    check(scalar(db, "SELECT COUNT(*) FROM artwork w JOIN albums a ON a.id=w.album_id WHERE a.name='ANThology'") == 0, "artwork removed with the album");

    /* Migration: an old v9 database (no trigger) with tagged songs gets its albums backfilled. */
    sql(db, "DROP TRIGGER songs_album_ai;DROP TRIGGER songs_album_au;DROP TRIGGER songs_album_ad;PRAGMA user_version=9;"
            "DELETE FROM albums;"
            "INSERT INTO songs(key, source, path, name, artist, album) VALUES('local:m1','local','m1','m','Moe','Old'),"
            "('local:m2','local','m2','m','Moe','Old'),('modland:m3','modland','m3','m','Moe','Old')");
    check(scalar(db, "SELECT COUNT(*) FROM albums") == 0, "old db has no albums");
    sel_library_close(lib);
    lib = sel_library_open(host, sel_machine_probe(host), 0, path);
    check(lib != NULL, "reopen migrates");
    if (lib == NULL) return 1;
    check(scalar(lib->db, "SELECT COUNT(*) FROM albums WHERE name='Old' AND artist='Moe'") == 2, "backfill: one row per source");
    check(listed(lib, "Old", NULL) == 2, "backfilled albums listed");
    sel_library_close(lib);

    if (fails == 0) printf("library_album_rows_ok\n");
    return fails != 0;
}
