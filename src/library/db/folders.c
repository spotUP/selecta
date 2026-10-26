/* Playlist folder tree (I178, design doc 9c: "user-organised hierarchy of playlists, stored only in the library
 * DB"). Tables `folders(id, name, parent_id)` and `playlists(id, name, folder_id)` from schema.c;
 * parent 0 / folder 0 is the root (NULL in the tables). Rules:
 *   - a folder moves under any folder except itself and its own descendants (SEL_FOLDER_ERR_CYCLE);
 *   - a folder with child folders or playlists is not deleted (SEL_FOLDER_ERR_NOT_EMPTY): the user moves the
 *     contents out first, so no delete ever loses a playlist. */
#include "folders.h"
#include "../song_key.h"

#include <stdio.h>
#include <string.h>

static sqlite3_stmt* prep(sqlite3* db, const char* sql) {
    sqlite3_stmt* st = NULL;
    return sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK ? st : NULL;
}

static void bind_id(sqlite3_stmt* st, int i, int64_t id) {
    if (id == 0) {
        sqlite3_bind_null(st, i);
    } else {
        sqlite3_bind_int64(st, i, id);
    }
}

/* First column of the first row of `sql` with ?1 = a, ?2 = b; -1 when there is no row. */
static int64_t scalar(sqlite3* db, const char* sql, int64_t a, int64_t b) {
    sqlite3_stmt* st = prep(db, sql);
    int64_t r = -1;
    if (st == NULL) return -1;
    sqlite3_bind_int64(st, 1, a);
    sqlite3_bind_int64(st, 2, b);
    if (sqlite3_step(st) == SQLITE_ROW) r = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return r;
}

static int folder_exists(sqlite3* db, int64_t id) {
    return id == 0 || scalar(db, "SELECT COUNT(*) FROM folders WHERE id = ?1", id, 0) == 1;
}

static int step_done(sqlite3_stmt* st) {
    int rc = st != NULL && sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    sqlite3_finalize(st);
    return rc;
}

static int refuse(int64_t* rows, int code) {
    *rows = code;
    return SQLITE_CONSTRAINT;
}

static int folder_add(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    if (op->name == NULL || op->name[0] == '\0') return SQLITE_MISUSE;
    if (!folder_exists(db, op->at)) return refuse(rows, SEL_FOLDER_ERR_NO_FOLDER);
    st = prep(db, "INSERT INTO folders(name, parent_id) VALUES(?1, ?2)");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_text(st, 1, op->name, -1, SQLITE_STATIC);
    bind_id(st, 2, op->at);
    if (step_done(st) != SQLITE_OK) return SQLITE_ERROR;
    *rows = sqlite3_last_insert_rowid(db);
    return SQLITE_OK;
}

static int folder_move(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    if (op->at <= 0 || !folder_exists(db, op->at) || !folder_exists(db, op->to)) {
        return refuse(rows, SEL_FOLDER_ERR_NO_FOLDER);
    }
    if (op->to != 0 &&
        scalar(db,
               "WITH RECURSIVE d(id) AS (SELECT ?1 UNION SELECT folders.id FROM folders JOIN d ON folders.parent_id = d.id)"
               " SELECT COUNT(*) FROM d WHERE id = ?2", op->at, op->to) > 0) {
        return refuse(rows, SEL_FOLDER_ERR_CYCLE);
    }
    st = prep(db, "UPDATE folders SET parent_id = ?2 WHERE id = ?1");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    bind_id(st, 2, op->to);
    return step_done(st);
}

static int folder_del(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    if (op->at <= 0 || !folder_exists(db, op->at)) return refuse(rows, SEL_FOLDER_ERR_NO_FOLDER);
    if (scalar(db, "SELECT (SELECT COUNT(*) FROM folders WHERE parent_id = ?1) + (SELECT COUNT(*) FROM playlists WHERE folder_id = ?1)",
               op->at, 0) != 0) {
        return refuse(rows, SEL_FOLDER_ERR_NOT_EMPTY);
    }
    st = prep(db, "DELETE FROM folders WHERE id = ?1");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    return step_done(st);
}

static int playlist_move(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    if (op->at <= 0 || scalar(db, "SELECT COUNT(*) FROM playlists WHERE id = ?1", op->at, 0) != 1) {
        return refuse(rows, SEL_FOLDER_ERR_NO_PLAYLIST);
    }
    if (!folder_exists(db, op->to)) return refuse(rows, SEL_FOLDER_ERR_NO_FOLDER);
    st = prep(db, "UPDATE playlists SET folder_id = ?2 WHERE id = ?1");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    bind_id(st, 2, op->to);
    return step_done(st);
}

/* name = title, at = folder, value = song keys separated by '\n' (may be NULL or empty). rows = new id. */
static int playlist_add(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    int64_t id;
    int64_t pos = 0;
    const char* p = op->value;
    if (op->name == NULL || op->name[0] == '\0') return SQLITE_MISUSE;
    if (!folder_exists(db, op->at)) return refuse(rows, SEL_FOLDER_ERR_NO_FOLDER);
    st = prep(db, "INSERT INTO playlists(name, folder_id) VALUES(?1, ?2)");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_text(st, 1, op->name, -1, SQLITE_STATIC);
    bind_id(st, 2, op->at);
    if (step_done(st) != SQLITE_OK) return SQLITE_ERROR;
    id = sqlite3_last_insert_rowid(db);
    while (p != NULL && *p != '\0') {
        const char* nl = strchr(p, '\n');
        size_t len = nl != NULL ? (size_t)(nl - p) : strlen(p);
        if (len > 0) {
            st = prep(db, "INSERT INTO playlist_entries(playlist_id, pos, key) VALUES(?1, ?2, ?3)");
            if (st == NULL) return SQLITE_ERROR;
            sqlite3_bind_int64(st, 1, id);
            sqlite3_bind_int64(st, 2, pos++);
            sqlite3_bind_text(st, 3, p, (int)len, SQLITE_STATIC);
            if (step_done(st) != SQLITE_OK) return SQLITE_ERROR;
        }
        p = nl != NULL ? nl + 1 : p + len;
    }
    *rows = id;
    return SQLITE_OK;
}

static int playlist_exists(sqlite3* db, int64_t id) {
    return id > 0 && scalar(db, "SELECT COUNT(*) FROM playlists WHERE id = ?1", id, 0) == 1;
}

/* at = playlist, value = one song key; appended after the last entry. */
static int playlist_add_song(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    if (op->value == NULL || op->value[0] == '\0') return SQLITE_MISUSE;
    if (!playlist_exists(db, op->at)) return refuse(rows, SEL_FOLDER_ERR_NO_PLAYLIST);
    st = prep(db, "INSERT INTO playlist_entries(playlist_id, pos, key)"
                  " VALUES(?1, (SELECT COALESCE(MAX(pos) + 1, 0) FROM playlist_entries WHERE playlist_id = ?1), ?2)");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    sqlite3_bind_text(st, 2, op->value, -1, SQLITE_STATIC);
    return step_done(st);
}

static int playlist_rename(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    if (op->name == NULL || op->name[0] == '\0') return SQLITE_MISUSE;
    if (!playlist_exists(db, op->at)) return refuse(rows, SEL_FOLDER_ERR_NO_PLAYLIST);
    st = prep(db, "UPDATE playlists SET name = ?2 WHERE id = ?1");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    sqlite3_bind_text(st, 2, op->name, -1, SQLITE_STATIC);
    return step_done(st);
}

static int playlist_del(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    if (!playlist_exists(db, op->at)) return refuse(rows, SEL_FOLDER_ERR_NO_PLAYLIST);
    st = prep(db, "DELETE FROM playlist_entries WHERE playlist_id = ?1");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    if (step_done(st) != SQLITE_OK) return SQLITE_ERROR;
    st = prep(db, "DELETE FROM playlists WHERE id = ?1");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    return step_done(st);
}

/* DUI-P4-18: the catalogue rows of one song. The album row and its artwork (cover and thumbnails) go with the last song of
 * that album. Playlists, history and the other user data are keyed by song key and stay. */
static int song_remove(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    static const char* const sql[] = {
        "DELETE FROM artwork WHERE album_id IN (SELECT a.id FROM albums a JOIN songs s ON a.name = s.album AND a.artist IS s.artist"
        " WHERE s.key = ?1 AND NOT EXISTS(SELECT 1 FROM songs o WHERE o.key <> ?1 AND o.album = a.name AND o.artist IS a.artist))",
        "DELETE FROM albums WHERE id IN (SELECT a.id FROM albums a JOIN songs s ON a.name = s.album AND a.artist IS s.artist"
        " WHERE s.key = ?1 AND NOT EXISTS(SELECT 1 FROM songs o WHERE o.key <> ?1 AND o.album = a.name AND o.artist IS a.artist))",
        "DELETE FROM song_composers WHERE song_key = ?1",
        "DELETE FROM files WHERE (source, path) IN (SELECT source, path FROM songs WHERE key = ?1)",
        "DELETE FROM songs WHERE key = ?1"};
    char key[SEL_SONG_KEY_MAX];
    size_t i;
    if (op->value == NULL || !sel_song_key_of(op->value, key, sizeof key)) return SQLITE_MISUSE;
    if (sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) return SQLITE_ERROR;
    for (i = 0; i < sizeof sql / sizeof sql[0]; i++) {
        sqlite3_stmt* st = NULL;
        int ok = sqlite3_prepare_v2(db, sql[i], -1, &st, NULL) == SQLITE_OK;
        if (ok) {
            sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
            ok = sqlite3_step(st) == SQLITE_DONE;
        }
        sqlite3_finalize(st);
        if (!ok) {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            return SQLITE_ERROR;
        }
    }
    *rows = sqlite3_changes(db);
    return sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK ? SQLITE_OK : SQLITE_ERROR;
}

/* DUI-P4-18 (Q16): Clear Library. Everything a scan or a sync put in the catalogue goes, and so does everything the user
 * keyed to those songs or built from them: playlists and their entries, folders, ratings, notes and the other per-song user
 * data. Settings (meta) stay. One transaction. */
static int library_clear(sqlite3* db, int64_t* rows) {
    int64_t n = scalar(db, "SELECT COUNT(*) FROM songs", 0, 0);
    if (sqlite3_exec(db, "BEGIN;DELETE FROM artwork;DELETE FROM albums;DELETE FROM song_composers;DELETE FROM files;DELETE FROM songs;"
                     "DELETE FROM playlist_entries;DELETE FROM playlists;DELETE FROM folders;DELETE FROM ratings;DELETE FROM notes;"
                     "DELETE FROM liked;DELETE FROM kept;DELETE FROM cues;DELETE FROM flags;DELETE FROM stickers;DELETE FROM history;COMMIT;",
                     NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return SQLITE_ERROR;
    }
    *rows = n > 0 ? n : 0;
    return SQLITE_OK;
}

/* DUI-P4-18: the catalogue title of one song (value = key or path); out = the title, rows = 1 when it has one. */
static int song_title(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    char key[SEL_SONG_KEY_MAX];
    sqlite3_stmt* st;
    *rows = 0;
    if (op->value == NULL || op->out == NULL || op->out_cap < 2u || !sel_song_key_of(op->value, key, sizeof key)) return SQLITE_MISUSE;
    op->out[0] = '\0';
    st = prep(db, "SELECT name FROM songs WHERE key = ?1 AND name IS NOT NULL AND name <> ''");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_text(st, 1, key, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) {
        snprintf(op->out, op->out_cap, "%s", (const char*)sqlite3_column_text(st, 0));
        *rows = 1;
    }
    sqlite3_finalize(st);
    return SQLITE_OK;
}

/* Appends `a` NUL `b` NUL to out; false when it does not fit (leaving room for the final empty string). */
static int put_pair(char* out, uint32_t cap, uint32_t* used, const char* a, const char* b) {
    size_t la = strlen(a) + 1u, lb = b != NULL ? strlen(b) + 1u : 0u;
    if (*used + la + lb + 1u > cap) return 0;
    memcpy(out + *used, a, la);
    if (b != NULL) memcpy(out + *used + la, b, lb);
    *used += (uint32_t)(la + lb);
    out[*used] = '\0';
    return 1;
}

/* out = "id\0name\0" for every playlist in folder `at` and its descendants (at 0 = every playlist), then "\0". */
static int folder_playlists(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    uint32_t used = 0;
    if (op->out == NULL || op->out_cap < 2u) return SQLITE_MISUSE;
    if (!folder_exists(db, op->at)) return refuse(rows, SEL_FOLDER_ERR_NO_FOLDER);
    op->out[0] = '\0';
    op->out[1] = '\0';
    st = prep(db, op->at == 0 ? "SELECT id, name FROM playlists ORDER BY id"
                              : "WITH RECURSIVE d(id) AS (SELECT ?1 UNION SELECT folders.id FROM folders JOIN d ON folders.parent_id = d.id)"
                                " SELECT id, name FROM playlists WHERE folder_id IN (SELECT id FROM d) ORDER BY id");
    if (st == NULL) return SQLITE_ERROR;
    if (op->at != 0) sqlite3_bind_int64(st, 1, op->at);
    *rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        char idtext[24];
        snprintf(idtext, sizeof idtext, "%lld", (long long)sqlite3_column_int64(st, 0));
        (*rows)++;
        put_pair(op->out, op->out_cap, &used, idtext, (const char*)sqlite3_column_text(st, 1));
    }
    sqlite3_finalize(st);
    return SQLITE_OK;
}

/* out = the song keys of playlist `at` in order, each NUL terminated, then "\0". */
static int playlist_keys(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    uint32_t used = 0;
    if (op->out == NULL || op->out_cap < 2u) return SQLITE_MISUSE;
    if (scalar(db, "SELECT COUNT(*) FROM playlists WHERE id = ?1", op->at, 0) != 1) {
        return refuse(rows, SEL_FOLDER_ERR_NO_PLAYLIST);
    }
    op->out[0] = '\0';
    op->out[1] = '\0';
    st = prep(db, "SELECT key FROM playlist_entries WHERE playlist_id = ?1 ORDER BY pos");
    if (st == NULL) return SQLITE_ERROR;
    sqlite3_bind_int64(st, 1, op->at);
    *rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        (*rows)++;
        put_pair(op->out, op->out_cap, &used, (const char*)sqlite3_column_text(st, 0), NULL);
    }
    sqlite3_finalize(st);
    return SQLITE_OK;
}

/* out = the tree as text, one line per node, depth-first, indented two spaces per level: "F <id> <name>" for a
 * folder (its sub-folders first, then its playlists), "P <id> <name>" for a playlist. rows = lines. */
static int folder_tree(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st;
    uint32_t used = 0;
    if (op->out == NULL || op->out_cap < 1u) return SQLITE_MISUSE;
    op->out[0] = '\0';
    st = prep(db,
              "WITH RECURSIVE t(id, name, depth, path) AS ("
              " SELECT id, name, 0, printf('%08d', id) FROM folders WHERE parent_id IS NULL"
              " UNION ALL SELECT f.id, f.name, t.depth + 1, t.path || '/' || printf('%08d', f.id)"
              " FROM folders f JOIN t ON f.parent_id = t.id)"
              " SELECT 'F', id, name, depth, path FROM t"
              " UNION ALL SELECT 'P', p.id, p.name, COALESCE(t.depth + 1, 0),"
              " COALESCE(t.path || '/', '') || 'p' || printf('%08d', p.id)"
              " FROM playlists p LEFT JOIN t ON t.id = p.folder_id ORDER BY 5");
    if (st == NULL) return SQLITE_ERROR;
    *rows = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        char line[320];
        int depth = sqlite3_column_int(st, 3);
        int n = snprintf(line, sizeof line, "%*s%s %lld %s\n", depth * 2, "", (const char*)sqlite3_column_text(st, 0),
                         (long long)sqlite3_column_int64(st, 1), (const char*)sqlite3_column_text(st, 2));
        (*rows)++;
        if (n > 0 && used + (uint32_t)n + 1u <= op->out_cap) {
            memcpy(op->out + used, line, (size_t)n + 1u);
            used += (uint32_t)n;
        }
    }
    sqlite3_finalize(st);
    return SQLITE_OK;
}

int sel_folder_exec(sqlite3* db, const sel_user_op* op, int64_t* rows, int32_t counts[4]) {
    *rows = 0;
    switch (op->op) {
    case SEL_USER_FOLDER_ADD: return folder_add(db, op, rows);
    case SEL_USER_FOLDER_MOVE: return folder_move(db, op, rows);
    case SEL_USER_FOLDER_DEL: return folder_del(db, op, rows);
    case SEL_USER_PLAYLIST_MOVE: return playlist_move(db, op, rows);
    case SEL_USER_PLAYLIST_ADD: return playlist_add(db, op, rows);
    case SEL_USER_PLAYLIST_ADD_SONG: return playlist_add_song(db, op, rows);
    case SEL_USER_PLAYLIST_RENAME: return playlist_rename(db, op, rows);
    case SEL_USER_PLAYLIST_DEL: return playlist_del(db, op, rows);
    case SEL_USER_SONG_REMOVE: return song_remove(db, op, rows);
    case SEL_USER_LIBRARY_COUNT:
        *rows = scalar(db, "SELECT COUNT(*) FROM songs", 0, 0);
        return *rows >= 0 ? SQLITE_OK : SQLITE_ERROR;
    case SEL_USER_LIBRARY_CLEAR: return library_clear(db, rows);
    case SEL_USER_SONG_TITLE: return song_title(db, op, rows);
    case SEL_USER_FOLDER_PLAYLISTS: return folder_playlists(db, op, rows);
    case SEL_USER_PLAYLIST_KEYS: return playlist_keys(db, op, rows);
    case SEL_USER_FOLDER_TREE: return folder_tree(db, op, rows);
    case SEL_USER_FOLDER_COUNTS:
        counts[0] = (int32_t)scalar(db, "SELECT COUNT(*) FROM folders", 0, 0);
        counts[1] = (int32_t)scalar(db, "SELECT COUNT(*) FROM playlists WHERE folder_id IS NOT NULL", 0, 0);
        counts[2] = (int32_t)scalar(db, "SELECT COUNT(*) FROM playlists", 0, 0);
        counts[3] = (int32_t)scalar(db, "SELECT COUNT(*) FROM playlist_entries", 0, 0);
        return SQLITE_OK;
    default: return SQLITE_MISUSE;
    }
}
