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

/* v4 (I192, F24): the statistics views. The stats screen reads only these views; no number is computed in C.
 * A play is a history row that was not skipped; its period is the calendar year (UTC) of played_at. Songs the
 * catalogue lacks count as format and artist "Unknown". Rank 1 is the most played, ties broken by name. */
static const char SCHEMA_V4[] =
    "CREATE VIEW stats_plays AS SELECT h.key AS key, strftime('%Y', h.played_at, 'unixepoch') AS period,"
    " COALESCE(h.duration_ms, 0) AS duration_ms, COALESCE(NULLIF(s.format, ''), 'Unknown') AS format,"
    " COALESCE(NULLIF(s.artist, ''), 'Unknown') AS artist"
    " FROM history h LEFT JOIN songs s ON s.key = h.key WHERE h.skipped = 0;"
    "CREATE VIEW stats_totals AS SELECT (SELECT COUNT(*) FROM songs) AS songs,"
    " (SELECT COUNT(*) FROM stats_plays) AS plays,"
    " (SELECT COALESCE(SUM(duration_ms), 0) FROM stats_plays) AS play_ms;"
    "CREATE VIEW stats_periods AS SELECT period, COUNT(*) AS plays, SUM(duration_ms) AS play_ms"
    " FROM stats_plays GROUP BY period;"
    "CREATE VIEW stats_format_counts AS SELECT period, format AS name, COUNT(*) AS plays"
    " FROM stats_plays GROUP BY period, format;"
    "CREATE VIEW stats_artist_counts AS SELECT period, artist AS name, COUNT(*) AS plays"
    " FROM stats_plays GROUP BY period, artist;"
    "CREATE VIEW stats_top_formats AS SELECT a.period AS period, a.name AS name, a.plays AS plays,"
    " (SELECT COUNT(*) FROM stats_format_counts b WHERE b.period = a.period"
    "  AND (b.plays > a.plays OR (b.plays = a.plays AND b.name < a.name))) + 1 AS rank FROM stats_format_counts a;"
    "CREATE VIEW stats_top_artists AS SELECT a.period AS period, a.name AS name, a.plays AS plays,"
    " (SELECT COUNT(*) FROM stats_artist_counts b WHERE b.period = a.period"
    "  AND (b.plays > a.plays OR (b.plays = a.plays AND b.name < a.name))) + 1 AS rank FROM stats_artist_counts a;";

/* v5 (I211): one song key everywhere. The canonical key is the source-prefixed key of design doc 9 (`local:<path>`,
 * the key the songs table, loudness and the stats and home views join on). History, liked, kept, ratings, cues,
 * notes, flags, stickers, pending pushes and playlist entries written by earlier builds under the plain local path
 * are rewritten to `local:<path>`. A URL (`scheme://`, a stream, I195) is its own identity and stays. A key that already starts with a built-in source id is left alone, so the script
 * is idempotent; where a prefixed row already exists the plain-path duplicate is dropped. The id list is the
 * built-in sources (source.c) frozen at this version. */
#define V5_RAW_KEY "instr(key, '://') = 0 AND (instr(key, ':') = 0 OR substr(key, 1, instr(key, ':') - 1) NOT IN ('local', 'modland', 'hvsc', 'hippo'))"
static const char SCHEMA_V5[] =
    "UPDATE history SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE liked SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM liked WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE kept SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM kept WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE ratings SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM ratings WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE cues SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM cues WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE notes SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM notes WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE flags SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM flags WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE stickers SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM stickers WHERE " V5_RAW_KEY ";"
    "UPDATE pending_push SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "UPDATE OR IGNORE playlist_entries SET key = 'local:' || key WHERE " V5_RAW_KEY ";"
    "DELETE FROM playlist_entries WHERE " V5_RAW_KEY ";";

/* v6 (I296): 1 marks a song_composers row written from the scanned file's author tag; the songdb sync replaces it. */
static const char SCHEMA_V6[] = "ALTER TABLE song_composers ADD COLUMN tag INTEGER NOT NULL DEFAULT 0;";

/* v7 (I528): the composer list as a maintained table. composers holds one row per canonical spelling: display is the MIN(display)
 * of its song_composers rows, songs the row count (song_composers is keyed by (song_key, canon), so a row is a distinct song).
 * composer_spellings counts the rows per (canon, display), so the MIN(display) after a delete is a seek among the few spellings
 * of one canon, not a scan of its song rows (and no new index on the 500 000-row song_composers, whose build does not fit the
 * SQLite heap). composer_unknown counts the songs rows without any song_composers row (the "Unknown composer" group).
 * Triggers keep all three current from every write path (scan, songdb sync, song remove, Clear Library), so the list is a plain
 * read. No path updates song_composers.song_key; the only songs.key update (the scan.c rename) is covered by the songs trigger. */
#define V7_ADD(r) \
    "INSERT INTO composer_spellings(canon, display, n) VALUES(" r ".canon, " r ".display, 1)" \
    " ON CONFLICT(canon, display) DO UPDATE SET n = n + 1;" \
    "INSERT INTO composers(canon, display, songs) VALUES(" r ".canon, " r ".display, 1)" \
    " ON CONFLICT(canon) DO UPDATE SET songs = songs + 1, display = MIN(display, excluded.display);"
#define V7_DROP(r) \
    "UPDATE composer_spellings SET n = n - 1 WHERE canon = " r ".canon AND display = " r ".display;" \
    "DELETE FROM composer_spellings WHERE canon = " r ".canon AND display = " r ".display AND n <= 0;" \
    "UPDATE composers SET songs = songs - 1,"  \
    " display = COALESCE((SELECT MIN(display) FROM composer_spellings WHERE canon = " r ".canon), display)" \
    " WHERE canon = " r ".canon;" \
    "DELETE FROM composers WHERE canon = " r ".canon AND songs <= 0;"
static const char SCHEMA_V7[] =
    "CREATE TABLE composers(canon TEXT PRIMARY KEY, display TEXT NOT NULL, songs INTEGER NOT NULL) WITHOUT ROWID;"
    "CREATE INDEX composers_by_name ON composers(display, canon, songs);"
    "CREATE TABLE composer_spellings(canon TEXT NOT NULL, display TEXT NOT NULL, n INTEGER NOT NULL,"
    " PRIMARY KEY(canon, display)) WITHOUT ROWID;"
    "CREATE TABLE composer_unknown(n INTEGER NOT NULL);"
    "INSERT INTO composer_spellings(canon, display, n) SELECT canon, display, 1 FROM song_composers WHERE 1"
    " ON CONFLICT(canon, display) DO UPDATE SET n = n + 1;"
    "INSERT INTO composers(canon, display, songs) SELECT canon, MIN(display), SUM(n) FROM composer_spellings GROUP BY canon;"
    "INSERT INTO composer_unknown(n) SELECT COUNT(*) FROM songs WHERE NOT EXISTS"
    " (SELECT 1 FROM song_composers c WHERE c.song_key = songs.key);"
    "CREATE TRIGGER song_composers_ai AFTER INSERT ON song_composers BEGIN "
    V7_ADD("new")
    "UPDATE composer_unknown SET n = n - 1 WHERE EXISTS (SELECT 1 FROM songs WHERE key = new.song_key)"
    " AND (SELECT COUNT(*) FROM song_composers WHERE song_key = new.song_key) = 1;"
    "END;"
    "CREATE TRIGGER song_composers_ad AFTER DELETE ON song_composers BEGIN "
    V7_DROP("old")
    "UPDATE composer_unknown SET n = n + 1 WHERE EXISTS (SELECT 1 FROM songs WHERE key = old.song_key)"
    " AND NOT EXISTS (SELECT 1 FROM song_composers WHERE song_key = old.song_key);"
    "END;"
    "CREATE TRIGGER song_composers_au AFTER UPDATE OF canon, display ON song_composers BEGIN "
    V7_DROP("old")
    V7_ADD("new")
    "END;"
    "CREATE TRIGGER songs_composers_ai AFTER INSERT ON songs BEGIN "
    "UPDATE composer_unknown SET n = n + 1 WHERE NOT EXISTS (SELECT 1 FROM song_composers WHERE song_key = new.key);"
    "END;"
    "CREATE TRIGGER songs_composers_ad AFTER DELETE ON songs BEGIN "
    "UPDATE composer_unknown SET n = n - 1 WHERE NOT EXISTS (SELECT 1 FROM song_composers WHERE song_key = old.key);"
    "END;"
    "CREATE TRIGGER songs_composers_au AFTER UPDATE OF key ON songs WHEN new.key <> old.key BEGIN "
    "UPDATE composer_unknown SET n = n - (NOT EXISTS (SELECT 1 FROM song_composers WHERE song_key = old.key))"
    " + (NOT EXISTS (SELECT 1 FROM song_composers WHERE song_key = new.key));"
    "END;";

/* v8 (I535): the songs count per source as a maintained table. The Sources page asks for every source's count on the UI
 * thread; COUNT(*) FROM songs WHERE source = ? has no index and scans the whole table per source (2.2 s for 8 sources on a
 * 520 000-song library). An index on songs(source) would fix the read, but its build has the same heap cost on the Amiga as
 * the v7 note above, so the count is kept by triggers like the v7 composer tables: the read is one primary key seek. */
#define V8_ADD(r) \
    "INSERT INTO source_songs(source, n) VALUES(" r ".source, 1) ON CONFLICT(source) DO UPDATE SET n = n + 1;"
#define V8_DROP(r) \
    "UPDATE source_songs SET n = n - 1 WHERE source = " r ".source;" \
    "DELETE FROM source_songs WHERE source = " r ".source AND n <= 0;"
static const char SCHEMA_V8[] =
    "CREATE TABLE source_songs(source TEXT PRIMARY KEY, n INTEGER NOT NULL) WITHOUT ROWID;"
    "INSERT INTO source_songs(source, n) SELECT source, 1 FROM songs WHERE 1 ON CONFLICT(source) DO UPDATE SET n = n + 1;"
    "CREATE TRIGGER songs_source_ai AFTER INSERT ON songs BEGIN " V8_ADD("new") "END;"
    "CREATE TRIGGER songs_source_ad AFTER DELETE ON songs BEGIN " V8_DROP("old") "END;"
    "CREATE TRIGGER songs_source_au AFTER UPDATE OF source ON songs WHEN new.source <> old.source BEGIN "
    V8_DROP("old") V8_ADD("new") "END;";

/* I678c: when a Your Library item (kind, id as in the item list) was last opened; Recents orders by the newer of this and the last play. */
static const char SCHEMA_V9[] =
    "CREATE TABLE opened(kind INTEGER NOT NULL, id TEXT NOT NULL, at INTEGER NOT NULL, PRIMARY KEY(kind, id)) WITHOUT ROWID;";

/* I678j: the format of a play, so a Modland or stream play (no songs row) still files under a Home format chip. A library play takes
 * songs.format; any other play takes the lower-case extension of its key (at most 5 characters). Earlier rows are filled the same way. */
#define V10_EXT "lower(substr(key, length(rtrim(key, replace(key, '.', ''))) + 1))"
static const char SCHEMA_V10[] =
    "ALTER TABLE history ADD COLUMN format TEXT;"
    "UPDATE history SET format = (SELECT s.format FROM songs s WHERE s.key = history.key);"
    "UPDATE history SET format = " V10_EXT " WHERE format IS NULL AND instr(key, '.') > 0"
    " AND length(" V10_EXT ") BETWEEN 1 AND 5 AND " V10_EXT " NOT GLOB '*[^a-z0-9]*';";

static const char* const g_migrations[] = {SCHEMA_V1, SCHEMA_V2, SCHEMA_V3, SCHEMA_V4, SCHEMA_V5,
                                           SCHEMA_V6, SCHEMA_V7, SCHEMA_V8, SCHEMA_V9, SCHEMA_V10};

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
