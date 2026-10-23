/* Schema v1 and user_version migrations (plan Phase 1, I146; design doc 9.2).
 * Each migration is one script that takes the DB from version N-1 to N; the
 * runner applies the missing ones inside one transaction each and sets
 * PRAGMA user_version. Add a version by appending to g_migrations. */
#include "schema.h"

#include <sqlite3.h>
#include <stddef.h>

static const char SCHEMA_V1[] =
    "CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;"
    "CREATE TABLE files(source TEXT NOT NULL, path TEXT NOT NULL, format TEXT, artist TEXT, album TEXT,"
    " name TEXT, hidden INTEGER NOT NULL DEFAULT 0, md5 TEXT, size INTEGER, PRIMARY KEY(source, path)) WITHOUT ROWID;"
    "CREATE TABLE songs(id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, source TEXT NOT NULL, path TEXT NOT NULL,"
    " name TEXT, artist TEXT, album TEXT, format TEXT, year INTEGER, genre TEXT, duration_ms INTEGER,"
    " loudness_gain INTEGER, md5 TEXT, size INTEGER, added INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE albums(id INTEGER PRIMARY KEY, name TEXT NOT NULL, artist TEXT, year INTEGER, source TEXT);"
    "CREATE TABLE artwork(album_id INTEGER NOT NULL, kind TEXT NOT NULL, data BLOB, PRIMARY KEY(album_id, kind));"
    "CREATE TABLE history(id INTEGER PRIMARY KEY, key TEXT NOT NULL, played_at INTEGER NOT NULL,"
    " duration_ms INTEGER, skipped INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX history_key ON history(key);"
    "CREATE TABLE liked(key TEXT PRIMARY KEY, at INTEGER NOT NULL) WITHOUT ROWID;"
    "CREATE TABLE kept(key TEXT PRIMARY KEY, at INTEGER NOT NULL) WITHOUT ROWID;"
    "CREATE TABLE ratings(key TEXT PRIMARY KEY, rating INTEGER NOT NULL, synced INTEGER NOT NULL DEFAULT 0) WITHOUT ROWID;"
    "CREATE TABLE cues(key TEXT NOT NULL, idx INTEGER NOT NULL, pos_ms INTEGER NOT NULL, label TEXT,"
    " PRIMARY KEY(key, idx)) WITHOUT ROWID;"
    "CREATE TABLE notes(key TEXT PRIMARY KEY, note TEXT NOT NULL) WITHOUT ROWID;"
    "CREATE TABLE flags(key TEXT PRIMARY KEY, wrong INTEGER NOT NULL DEFAULT 0, skip INTEGER NOT NULL DEFAULT 0,"
    " note TEXT) WITHOUT ROWID;"
    "CREATE TABLE stickers(key TEXT NOT NULL, name TEXT NOT NULL, value TEXT, PRIMARY KEY(key, name)) WITHOUT ROWID;"
    "CREATE TABLE genre_cache(key TEXT PRIMARY KEY, genre TEXT, fetched INTEGER NOT NULL DEFAULT 0) WITHOUT ROWID;"
    "CREATE TABLE pending_push(id INTEGER PRIMARY KEY, kind TEXT NOT NULL, key TEXT, payload TEXT,"
    " created INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE folders(id INTEGER PRIMARY KEY, name TEXT NOT NULL, parent_id INTEGER REFERENCES folders(id));"
    "CREATE TABLE playlists(id INTEGER PRIMARY KEY, name TEXT NOT NULL, folder_id INTEGER REFERENCES folders(id));"
    "CREATE TABLE playlist_entries(playlist_id INTEGER NOT NULL, pos INTEGER NOT NULL, key TEXT NOT NULL,"
    " PRIMARY KEY(playlist_id, pos)) WITHOUT ROWID;"
    "CREATE TABLE views(name TEXT PRIMARY KEY, query TEXT NOT NULL) WITHOUT ROWID;"
    /* FTS5 prefix search over name, artist, format, path; external content = songs. */
    "CREATE VIRTUAL TABLE song_fts USING fts5(name, artist, format, path, content='songs', content_rowid='id',"
    " prefix='2 3');"
    "CREATE TRIGGER songs_ai AFTER INSERT ON songs BEGIN"
    " INSERT INTO song_fts(rowid, name, artist, format, path) VALUES(new.id, new.name, new.artist, new.format, new.path);"
    "END;"
    "CREATE TRIGGER songs_ad AFTER DELETE ON songs BEGIN"
    " INSERT INTO song_fts(song_fts, rowid, name, artist, format, path)"
    " VALUES('delete', old.id, old.name, old.artist, old.format, old.path);"
    "END;"
    "CREATE TRIGGER songs_au AFTER UPDATE ON songs BEGIN"
    " INSERT INTO song_fts(song_fts, rowid, name, artist, format, path)"
    " VALUES('delete', old.id, old.name, old.artist, old.format, old.path);"
    " INSERT INTO song_fts(rowid, name, artist, format, path) VALUES(new.id, new.name, new.artist, new.format, new.path);"
    "END;";

/* v2 (I182): DEViLBOX songdb answers by UADE key (authors one per line, NULL = not in songdb) and the composer
 * rows derived from them per song key (canon from library/composer.h, display for the composer page). */
static const char SCHEMA_V2[] =
    "CREATE TABLE songdb_cache(key TEXT PRIMARY KEY, authors TEXT, fetched INTEGER NOT NULL DEFAULT 0) WITHOUT ROWID;"
    "CREATE TABLE song_composers(song_key TEXT NOT NULL, canon TEXT NOT NULL, display TEXT NOT NULL,"
    " PRIMARY KEY(song_key, canon)) WITHOUT ROWID;"
    "CREATE INDEX song_composers_canon ON song_composers(canon);";

/* v3 (I190): 1 marks songs.loudness_gain as the adaptive leveler's provisional estimate; a real gain clears it. */
static const char SCHEMA_V3[] = "ALTER TABLE songs ADD COLUMN loudness_prov INTEGER NOT NULL DEFAULT 0;";

static const char* const g_migrations[] = {SCHEMA_V1, SCHEMA_V2, SCHEMA_V3};

#define MIGRATION_COUNT ((int)(sizeof g_migrations / sizeof g_migrations[0]))

int sel_schema_version(sqlite3* db) {
    sqlite3_stmt* st = NULL;
    int v = -1;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) {
            v = sqlite3_column_int(st, 0);
        }
    }
    sqlite3_finalize(st);
    return v;
}

int sel_schema_migrate(sqlite3* db) {
    int v = sel_schema_version(db);
    char pragma[40];
    int i;

    if (v < 0 || v > MIGRATION_COUNT) { /* newer DB than this build: refuse, never downgrade */
        return 0;
    }
    for (i = v; i < MIGRATION_COUNT; i++) {
        if (sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) {
            return 0;
        }
        if (sqlite3_exec(db, g_migrations[i], NULL, NULL, NULL) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            return 0;
        }
        sqlite3_snprintf((int)sizeof pragma, pragma, "PRAGMA user_version=%d", i + 1);
        if (sqlite3_exec(db, pragma, NULL, NULL, NULL) != SQLITE_OK ||
            sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            return 0;
        }
    }
    return 1;
}
