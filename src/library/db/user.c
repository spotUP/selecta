/* User data writes and reads (plan 2026-10-25-m3-library.md Phase 6, I170): finished plays and skips in
 * `history`, LIKE and KEEP in `liked` and `kept`, per-song replayer options as the sticker "rp.config".
 * One request, SEL_LIB_REQ_USER, runs on the worker like every other library request. */
#include "db_internal.h"
#include "folders.h"

#include <stdio.h>
#include <string.h>

static int user_exec(sqlite3* db, const char* sql, const sel_user_op* op, int shape) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    if (shape == 1) { /* history: played_at, skipped, duration */
        sqlite3_bind_int64(st, 2, op->at);
        sqlite3_bind_int(st, 3, op->skipped != 0);
        if (op->duration_ms > 0) sqlite3_bind_int(st, 4, op->duration_ms);
    } else if (shape == 2) { /* liked, kept */
        sqlite3_bind_int64(st, 2, op->at);
    } else if (shape == 3) { /* sticker set */
        sqlite3_bind_text(st, 2, op->name, -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 3, op->value, -1, SQLITE_STATIC);
    } else if (shape == 4) { /* sticker delete */
        sqlite3_bind_text(st, 2, op->name, -1, SQLITE_STATIC);
    }
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int64_t flag_rows(sqlite3* db) {
    sqlite3_stmt* st = NULL;
    int64_t n = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM flags", -1, &st, NULL) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
        sqlite3_finalize(st);
    }
    return n;
}

/* sounds-wrong.txt: one "key<TAB>note" line per wrong-flagged song, in key order. */
static int flags_export(sqlite3* db, const char* path, int64_t* lines) {
    sqlite3_stmt* st = NULL;
    FILE* f;
    int rc = sqlite3_prepare_v2(db, "SELECT key, COALESCE(note, '') FROM flags WHERE wrong = 1 ORDER BY key", -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    f = fopen(path, "w");
    if (f == NULL) {
        sqlite3_finalize(st);
        return SQLITE_CANTOPEN;
    }
    *lines = 0;
    while (sqlite3_step(st) == SQLITE_ROW) {
        fprintf(f, "%s\t%s\n", (const char*)sqlite3_column_text(st, 0), (const char*)sqlite3_column_text(st, 1));
        (*lines)++;
    }
    sqlite3_finalize(st);
    return fclose(f) == 0 ? SQLITE_OK : SQLITE_IOERR;
}

/* Runs `sql` with up to three text parameters; a NULL argument binds nothing. */
static int exec_text3(sqlite3* db, const char* sql, const char* a, const char* b, const char* c) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    if (a != NULL) sqlite3_bind_text(st, 1, a, -1, SQLITE_STATIC);
    if (b != NULL) sqlite3_bind_text(st, 2, b, -1, SQLITE_STATIC);
    if (c != NULL) sqlite3_bind_text(st, 3, c, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

/* NOTE: the note replaces the song's row in `notes` and a "comment" row joins pending_push (the optional
 * DEViLBOX push drains that queue; this library never sends anything itself). */
static int note_set(sqlite3* db, const char* key, const char* text) {
    int rc;
    if (text == NULL || text[0] == '\0') return exec_text3(db, "DELETE FROM notes WHERE key = ?1", key, NULL, NULL);
    rc = exec_text3(db, "INSERT OR REPLACE INTO notes(key, note) VALUES(?1, ?2)", key, text, NULL);
    if (rc != SQLITE_OK) return rc;
    return exec_text3(db,
                      "INSERT INTO pending_push(kind, key, payload, created) VALUES('comment', ?1, ?2,"
                      " CAST(strftime('%s', 'now') AS INTEGER))", key, text, NULL);
}

/* TAG_EDIT: the edit lives in the sticker "tag.<field>" (re-applied by SEL_SONG_TAG_KEEP on every upsert) and is
 * written to the songs row now, so queries, FTS and display all read the one songs column. */
static int tag_edit(sqlite3* db, const char* key, const char* field, const char* value) {
    static const char* const fields[] = {"artist", "name", "album", "genre"};
    char sticker[24];
    char sql[96];
    size_t i;
    int rc;

    for (i = 0; i < sizeof fields / sizeof fields[0]; i++) {
        if (strcmp(field, fields[i]) == 0) break;
    }
    if (i == sizeof fields / sizeof fields[0]) return SQLITE_MISUSE;
    snprintf(sticker, sizeof sticker, "tag.%s", field);
    if (value == NULL || value[0] == '\0') {
        return exec_text3(db, "DELETE FROM stickers WHERE key = ?1 AND name = ?2", key, sticker, NULL);
    }
    rc = exec_text3(db, "INSERT OR REPLACE INTO stickers(key, name, value) VALUES(?1, ?2, ?3)", key, sticker, value);
    if (rc != SQLITE_OK) return rc;
    snprintf(sql, sizeof sql, "UPDATE songs SET %s = ?2 WHERE key = ?1", field);
    return exec_text3(db, sql, key, value, NULL);
}

/* I185: appends one "label\0value\0" pair to a panel list unless the value is absent, empty or "0"; the list
 * always ends in an empty label. Returns 1 when a row was added. */
static int panel_put(char* out, uint32_t cap, uint32_t* used, const char* label, const char* value) {
    size_t ll = strlen(label) + 1u, lv;
    if (value == NULL || value[0] == '\0' || strcmp(value, "0") == 0) return 0;
    lv = strlen(value) + 1u;
    if (*used + ll + lv + 1u > cap) return 0;
    memcpy(out + *used, label, ll);
    memcpy(out + *used + ll, value, lv);
    *used += (uint32_t)(ll + lv);
    out[*used] = '\0';
    return 1;
}

static void user_counts(sqlite3* db, int32_t out[4]) {
    static const char* const tables[4] = {"history", "flags", "notes", "pending_push"};
    int i;
    for (i = 0; i < 4; i++) {
        sqlite3_stmt* st = NULL;
        char sql[48];
        snprintf(sql, sizeof sql, "SELECT COUNT(*) FROM %s", tables[i]);
        out[i] = 0;
        if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
            if (sqlite3_step(st) == SQLITE_ROW) out[i] = sqlite3_column_int(st, 0);
            sqlite3_finalize(st);
        }
    }
}

void sel_library_user(sel_library* lib, const sel_user_op* op, sel_message* reply) {
    int rc = SQLITE_MISUSE;
    int found = 0;
    int64_t rows = -1;
    int32_t counts[4] = {0, 0, 0, 0};
    int have_counts = 0;
    int i;

    if (op->op >= SEL_USER_FOLDER_ADD && op->op <= SEL_USER_FOLDER_TREE) {
        rc = sel_folder_exec(lib->db, op, &rows, counts);
        have_counts = op->op == SEL_USER_FOLDER_COUNTS;
    } else if (op->op == SEL_USER_COUNTS) {
        user_counts(lib->db, counts);
        have_counts = 1;
        rc = SQLITE_OK;
    } else if (op->op == SEL_USER_FLAG_EXPORT && op->name != NULL) {
        rows = 0;
        rc = flags_export(lib->db, op->name, &rows);
    } else if (op->key == NULL || op->key[0] == '\0') {
        /* rc stays SQLITE_MISUSE */
    } else if (op->op == SEL_USER_NOTE) {
        rc = note_set(lib->db, op->key, op->value);
    } else if (op->op == SEL_USER_TAG_EDIT && op->name != NULL) {
        rc = tag_edit(lib->db, op->key, op->name, op->value);
    } else if (op->op == SEL_USER_FLAG && op->name != NULL && strcmp(op->name, "wrong") == 0) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db,
                                "INSERT INTO flags(key, wrong, note) VALUES(?1, 1, ?2) "
                                "ON CONFLICT(key) DO UPDATE SET wrong = 1, note = excluded.note", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_text(st, 2, op->value != NULL ? op->value : "", -1, SQLITE_STATIC);
            rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
            sqlite3_finalize(st);
        }
        rows = flag_rows(lib->db);
    } else if (op->op == SEL_USER_FLAG && op->name != NULL && strcmp(op->name, "skip") == 0) {
        rc = user_exec(lib->db, "INSERT INTO flags(key, skip) VALUES(?1, 1) ON CONFLICT(key) DO UPDATE SET skip = 1", op, 0);
        rows = flag_rows(lib->db);
    } else if (op->op == SEL_USER_FLAG_SKIPPED) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db, "SELECT 1 FROM flags WHERE key = ?1 AND skip = 1", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            found = sqlite3_step(st) == SQLITE_ROW;
            sqlite3_finalize(st);
        }
    } else if (op->op == SEL_USER_HISTORY) {
        rc = user_exec(lib->db, "INSERT INTO history(key, played_at, skipped, duration_ms) VALUES(?1, ?2, ?3, ?4)", op, 1);
    } else if (op->op == SEL_USER_LIKE) {
        rc = user_exec(lib->db, "INSERT OR REPLACE INTO liked(key, at) VALUES(?1, ?2)", op, 2);
    } else if (op->op == SEL_USER_KEEP) {
        rc = user_exec(lib->db, "INSERT OR REPLACE INTO kept(key, at) VALUES(?1, ?2)", op, 2);
    } else if (op->op == SEL_USER_STICKER_SET && op->name != NULL && op->value != NULL) {
        rc = user_exec(lib->db, "INSERT OR REPLACE INTO stickers(key, name, value) VALUES(?1, ?2, ?3)", op, 3);
    } else if (op->op == SEL_USER_STICKER_GET && op->name != NULL && op->out != NULL && op->out_cap > 0) {
        sqlite3_stmt* st = NULL;
        op->out[0] = '\0';
        rc = sqlite3_prepare_v2(lib->db, "SELECT value FROM stickers WHERE key = ?1 AND name = ?2", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_text(st, 2, op->name, -1, SQLITE_STATIC);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char* v = sqlite3_column_text(st, 0);
                if (v != NULL) {
                    snprintf(op->out, op->out_cap, "%s", (const char*)v);
                    found = 1;
                }
            }
            sqlite3_finalize(st);
            rc = SQLITE_OK;
        }
    } else if (op->op == SEL_USER_STICKER_DEL && op->name != NULL) {
        rc = user_exec(lib->db, "DELETE FROM stickers WHERE key = ?1 AND name = ?2", op, 4);
        found = rc == SQLITE_OK && sqlite3_changes(lib->db) > 0;
    } else if (op->op == SEL_USER_STICKER_LIST && op->out != NULL && op->out_cap > 0) {
        sqlite3_stmt* st = NULL;
        uint32_t used = 0;
        op->out[0] = '\0';
        rows = 0;
        rc = sqlite3_prepare_v2(lib->db, "SELECT name, COALESCE(value, '') FROM stickers WHERE key = ?1 ORDER BY name",
                                -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const char* n = (const char*)sqlite3_column_text(st, 0);
                const char* v = (const char*)sqlite3_column_text(st, 1);
                size_t ln = strlen(n) + 1u, lv = strlen(v) + 1u;
                rows++;
                if (used + ln + lv + 1u <= op->out_cap) { /* +1: the final empty name ends the list */
                    memcpy(op->out + used, n, ln);
                    memcpy(op->out + used + ln, v, lv);
                    used += (uint32_t)(ln + lv);
                    op->out[used] = '\0';
                }
            }
            sqlite3_finalize(st);
        }
    } else if (op->op == SEL_USER_PANEL && op->out != NULL && op->out_cap > 0) {
        /* I185: the catalogue rows of the module info panel, in display order; absent data adds no row. */
        static const char* const labels[] = {"STIL note", "Composer", "Release", "Year", "Genre", "Rating", "Play count"};
        sqlite3_stmt* st = NULL;
        uint32_t used = 0;
        op->out[0] = '\0';
        rows = 0;
        rc = sqlite3_prepare_v2(
            lib->db,
            "SELECT (SELECT value FROM stickers WHERE key = ?1 AND name = 'stil.comment'),"
            " (SELECT group_concat(display, ', ') FROM (SELECT display FROM song_composers WHERE song_key = ?1"
            "  ORDER BY display)),"
            " s.album, s.year, s.genre, (SELECT rating FROM ratings WHERE key = ?1),"
            " (SELECT COUNT(*) FROM history WHERE key = ?1 AND skipped = 0)"
            " FROM (SELECT ?1 AS k) q LEFT JOIN songs s ON s.key = q.k",
            -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            if (sqlite3_step(st) == SQLITE_ROW) {
                int c;
                for (c = 0; c < 7; c++) {
                    rows += (int64_t)panel_put(op->out, op->out_cap, &used, labels[c],
                                               (const char*)sqlite3_column_text(st, c));
                }
            }
            sqlite3_finalize(st);
        }
    } else if (op->op == SEL_USER_CUE_SET) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db, "INSERT OR REPLACE INTO cues(key, idx, pos_ms, label) VALUES(?1, ?2, ?3, ?4)",
                                -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 2, op->at);
            sqlite3_bind_int(st, 3, op->duration_ms);
            if (op->name != NULL) sqlite3_bind_text(st, 4, op->name, -1, SQLITE_STATIC);
            rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
            sqlite3_finalize(st);
        }
    } else if (op->op == SEL_USER_CUE_GET) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db, "SELECT pos_ms FROM cues WHERE key = ?1 AND idx = ?2", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 2, op->at);
            if (sqlite3_step(st) == SQLITE_ROW) {
                rows = sqlite3_column_int64(st, 0);
                found = 1;
            }
            sqlite3_finalize(st);
        }
    } else if (op->op == SEL_USER_LOUDNESS_SET) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db, "UPDATE songs SET loudness_gain = ?2 WHERE key = ?1", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 2, op->at);
            rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
            sqlite3_finalize(st);
        }
        if (rc == SQLITE_OK && sqlite3_changes(lib->db) == 0) {
            st = NULL;
            rc = sqlite3_prepare_v2(lib->db,
                                    "INSERT INTO songs(key, source, path, loudness_gain) VALUES(?1, 'local', ?3, ?2)",
                                    -1, &st, NULL);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
                sqlite3_bind_int64(st, 2, op->at);
                sqlite3_bind_text(st, 3, op->value != NULL ? op->value : op->key, -1, SQLITE_STATIC);
                rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
                sqlite3_finalize(st);
            }
        }
    } else if (op->op == SEL_USER_LOUDNESS_GET) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db, "SELECT loudness_gain FROM songs WHERE key = ?1", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL) {
                rows = sqlite3_column_int64(st, 0);
                found = 1;
            }
            sqlite3_finalize(st);
        }
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_USER;
    reply->argc = have_counts ? 6 : 3;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = rc == SQLITE_OK ? 1 + found : 0;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
    reply->args[2].type = SEL_ARG_INT; /* FLAG: rows in `flags`; FLAG_EXPORT: lines written */
    reply->args[2].v.i = (int32_t)rows;
    if (have_counts) { /* COUNTS: [2..5] = history, flags, notes, pending_push */
        for (i = 0; i < 4; i++) {
            reply->args[2 + i].type = SEL_ARG_INT;
            reply->args[2 + i].v.i = counts[i];
        }
    }
}
