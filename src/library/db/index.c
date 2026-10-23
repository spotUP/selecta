/* Prebuilt read-only source index (D-M3-16, I197). The desktop builds it with `selecta-cli library
 * export-index <source>`: a database file with the full library schema holding the songs of one source,
 * FTS index included. Every machine ATTACHes it read-only as schema `idx` beside the writable user DB;
 * search.c and the stats handler query both. User data (history, ratings, notes) stays in the main DB and
 * is keyed by song key, so it joins an index row the same way it joins a scanned one. */
#include "db_internal.h"
#include "schema.h"

#include <stdio.h>
#include <string.h>

uint32_t sel_library_heap_peak(void) {
    sqlite3_int64 cur = 0, hi = 0;

    sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &cur, &hi, 0);
    return (uint32_t)hi;
}

uint32_t sel_library_index_songs(sel_library* lib) {
    sqlite3_stmt* st = NULL;
    uint32_t n = 0;

    if (!lib->has_index || sqlite3_prepare_v2(lib->db, "SELECT count(*) FROM idx.songs", -1, &st, NULL) != SQLITE_OK) {
        return 0;
    }
    if (sqlite3_step(st) == SQLITE_ROW) {
        n = (uint32_t)sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    return n;
}

int sel_library_attach_index(sel_library* lib, const char* path) {
    char sql[600];
    char uri[500];
    size_t o;

    if (lib->has_index || path == NULL) {
        return 0;
    }
    /* A file: URI is the only way to open an ATTACHed file read-only; escape the characters it reserves. */
    memcpy(uri, "file:", 5);
    o = 5;
    for (; *path != '\0' && o + 4 < sizeof uri; path++) {
        if (*path == '%' || *path == '?' || *path == '#') {
            o += (size_t)snprintf(uri + o, sizeof uri - o, "%%%02X", (unsigned)(unsigned char)*path);
        } else {
            uri[o++] = *path;
        }
    }
    uri[o] = '\0';
    sqlite3_snprintf((int)sizeof sql, sql, "ATTACH DATABASE '%q?mode=ro' AS idx", uri);
    if (sqlite3_exec(lib->db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        return 0;
    }
    /* Refuse a file that is not a library database: the first query would otherwise fail much later. */
    lib->has_index = 1;
    if (sqlite3_exec(lib->db, "SELECT 1 FROM idx.songs LIMIT 1", NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_exec(lib->db, "DETACH DATABASE idx", NULL, NULL, NULL);
        lib->has_index = 0;
        return 0;
    }
    return 1;
}

int sel_library_export_index(sel_library* lib, const char* source, const char* out_path, uint32_t* rows) {
    sqlite3* fresh = NULL;
    sqlite3_stmt* st = NULL;
    char sql[600];
    int ok = 0;

    if (rows != NULL) {
        *rows = 0;
    }
    if (source == NULL || out_path == NULL || lib->has_index) {
        return 0;
    }
    remove(out_path);
    if (sqlite3_open_v2(out_path, &fresh, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL) !=
            SQLITE_OK ||
        !sel_schema_migrate(fresh)) {
        sqlite3_close(fresh);
        return 0;
    }
    sqlite3_close(fresh);
    sqlite3_snprintf((int)sizeof sql, sql, "ATTACH DATABASE '%q' AS ex", out_path);
    if (sqlite3_exec(lib->db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        return 0;
    }
    if (sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(lib->db, "INSERT INTO ex.songs SELECT * FROM main.songs WHERE source = ?1", -1, &st,
                           NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, source, -1, SQLITE_STATIC);
        ok = sqlite3_step(st) == SQLITE_DONE;
        if (ok && rows != NULL) {
            *rows = (uint32_t)sqlite3_changes(lib->db);
        }
    }
    sqlite3_finalize(st);
    ok = ok && sqlite3_exec(lib->db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK &&
         sqlite3_exec(lib->db, "INSERT INTO ex.song_fts(song_fts) VALUES('optimize')", NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) {
        sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL);
    }
    sqlite3_exec(lib->db, "DETACH DATABASE ex", NULL, NULL, NULL);
    return ok;
}
