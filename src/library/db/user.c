/* User data writes and reads (plan 2026-10-25-m3-library.md Phase 6, I170): finished plays and skips in
 * `history`, LIKE and KEEP in `liked` and `kept`, per-song replayer options as the sticker "rp.config".
 * One request, SEL_LIB_REQ_USER, runs on the worker like every other library request. */
#include "db_internal.h"
#include "folders.h"

#include <stdio.h>
#include <stdlib.h>
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

/* I192 (F24): appends one "label\0value\0" pair of the stats screen; unlike panel_put a zero is a value. */
static void stats_put(char* out, uint32_t cap, uint32_t* used, uint32_t* rows, const char* label, const char* value) {
    size_t ll = strlen(label) + 1u, lv;
    if (value == NULL) return;
    lv = strlen(value) + 1u;
    if (*used + ll + lv + 1u > cap) return;
    memcpy(out + *used, label, ll);
    memcpy(out + *used + ll, value, lv);
    *used += (uint32_t)(ll + lv);
    out[*used] = '\0';
    (*rows)++;
}

/* Runs `sql` (bind ?1 = period when non-NULL) and adds one row per result row: column 0 = label, column 1 = value. */
static void stats_rows(sqlite3* db, const char* sql, const char* period, char* out, uint32_t cap, uint32_t* used,
                       uint32_t* rows) {
    sqlite3_stmt* st = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK) return;
    if (period != NULL) sqlite3_bind_text(st, 1, period, -1, SQLITE_STATIC);
    while (sqlite3_step(st) == SQLITE_ROW) {
        stats_put(out, cap, used, rows, (const char*)sqlite3_column_text(st, 0), (const char*)sqlite3_column_text(st, 1));
    }
    sqlite3_finalize(st);
}

#define STATS_HMS(col) "printf('%d:%02d:%02d', " col " / 1000 / 3600, " col " / 1000 % 3600 / 60, " col " / 1000 % 60)"

/* The stats screen text: every number comes out of the stats_* views (schema v4). Totals, then for the newest
 * three periods the plays and play time and the top two formats and artists. */
static int64_t stats_build(sqlite3* db, char* out, uint32_t cap) {
    sqlite3_stmt* st = NULL;
    uint32_t used = 0, rows = 0;
    char periods[3][8];
    int n = 0, i;
    out[0] = '\0';
    stats_rows(db, "SELECT 'Songs in library', songs FROM stats_totals UNION ALL SELECT 'Total plays', plays FROM stats_totals"
                   " UNION ALL SELECT 'Total play time', " STATS_HMS("play_ms") " FROM stats_totals",
               NULL, out, cap, &used, &rows);
    if (sqlite3_prepare_v2(db, "SELECT period FROM stats_periods ORDER BY period DESC LIMIT 3", -1, &st, NULL) == SQLITE_OK) {
        while (n < 3 && sqlite3_step(st) == SQLITE_ROW) {
            snprintf(periods[n++], sizeof periods[0], "%s", (const char*)sqlite3_column_text(st, 0));
        }
        sqlite3_finalize(st);
    }
    for (i = 0; i < n; i++) {
        stats_rows(db, "SELECT period || ' plays', plays || ' plays, ' || " STATS_HMS("play_ms")
                       " FROM stats_periods WHERE period = ?1",
                   periods[i], out, cap, &used, &rows);
        stats_rows(db, "SELECT period || ' format ' || rank, name || ' (' || plays || ')' FROM stats_top_formats"
                       " WHERE period = ?1 AND rank <= 2 ORDER BY rank",
                   periods[i], out, cap, &used, &rows);
        stats_rows(db, "SELECT period || ' artist ' || rank, name || ' (' || plays || ')' FROM stats_top_artists"
                       " WHERE period = ?1 AND rank <= 2 ORDER BY rank",
                   periods[i], out, cap, &used, &rows);
    }
    return rows;
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

/* RATING_SET: the ratings row, plus (SEL_RATING_QUEUE) the one pending "rating" push of the key. A rating that
 * came from the server (SEL_RATING_SERVER) never overwrites a local rating still waiting to be pushed. */
static int rating_set(sqlite3* db, const sel_user_op* op) {
    sqlite3_stmt* st = NULL;
    int rc;

    if (op->to == SEL_RATING_SERVER) {
        int waiting;
        rc = sqlite3_prepare_v2(db, "SELECT 1 FROM pending_push WHERE kind = 'rating' AND key = ?1", -1, &st, NULL);
        if (rc != SQLITE_OK) return rc;
        sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
        waiting = sqlite3_step(st) == SQLITE_ROW;
        sqlite3_finalize(st);
        if (waiting) return SQLITE_OK;
    }
    if (op->at <= 0) {
        rc = user_exec(db, "DELETE FROM ratings WHERE key = ?1", op, 0);
    } else {
        rc = sqlite3_prepare_v2(db,
                                "INSERT INTO ratings(key, rating, synced) VALUES(?1, ?2, ?3) "
                                "ON CONFLICT(key) DO UPDATE SET rating = excluded.rating, synced = excluded.synced",
                                -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 2, op->at);
            sqlite3_bind_int(st, 3, op->to == SEL_RATING_SERVER ? 1 : 0);
            rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
            sqlite3_finalize(st);
        }
    }
    if (rc == SQLITE_OK && op->to == SEL_RATING_QUEUE) {
        rc = user_exec(db, "DELETE FROM pending_push WHERE kind = 'rating' AND key = ?1", op, 0);
        st = NULL;
        if (rc == SQLITE_OK) {
            rc = sqlite3_prepare_v2(db,
                                    "INSERT INTO pending_push(kind, key, payload, created) VALUES('rating', ?1, ?2,"
                                    " CAST(strftime('%s', 'now') AS INTEGER))",
                                    -1, &st, NULL);
        }
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 2, op->at < 0 ? 0 : op->at);
            rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
            sqlite3_finalize(st);
        }
    }
    return rc;
}

/* RATING_PENDING: "key\0stars\0" pairs in push order that fit, then "\0"; rows = all pending ratings. */
static int rating_pending(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    sqlite3_stmt* st = NULL;
    uint32_t used = 0;
    int rc;

    op->out[0] = '\0';
    *rows = 0;
    rc = sqlite3_prepare_v2(db, "SELECT key, COALESCE(payload, '0') FROM pending_push WHERE kind = 'rating' ORDER BY id",
                            -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* k = (const char*)sqlite3_column_text(st, 0);
        const char* v = (const char*)sqlite3_column_text(st, 1);
        size_t lk = strlen(k) + 1u, lv = strlen(v) + 1u;

        (*rows)++;
        if (used + lk + lv + 1u <= op->out_cap) {
            memcpy(op->out + used, k, lk);
            memcpy(op->out + used + lk, v, lv);
            used += (uint32_t)(lk + lv);
            op->out[used] = '\0';
        }
    }
    sqlite3_finalize(st);
    return SQLITE_OK;
}

/* Appends one home line ("<tag> <text>\n") when it fits; counts it either way. */
static void home_put(const sel_user_op* op, uint32_t* used, int64_t* rows, const char* tag, const char* text) {
    size_t n = strlen(tag) + 1u + strlen(text) + 1u;
    (*rows)++;
    if (*used + n + 1u <= op->out_cap) {
        *used += (uint32_t)snprintf(op->out + *used, op->out_cap - *used, "%s %s\n", tag, text);
    }
}

/* Runs `sql` (one text column, an optional second one for the artist) and puts every row as an `S` line. */
static int home_list(sqlite3* db, const sel_user_op* op, uint32_t* used, int64_t* rows, const char* sql) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int64(st, 1, op->at > 0 ? op->at : 10);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(st, 0);
        const char* artist = (const char*)sqlite3_column_text(st, 1);
        char line[160];
        if (artist != NULL && artist[0] != '\0') {
            snprintf(line, sizeof line, "%s - %s", name, artist);
        } else {
            snprintf(line, sizeof line, "%s", name);
        }
        home_put(op, used, rows, "S", line);
    }
    sqlite3_finalize(st);
    return SQLITE_OK;
}

/* I191: the home view text. */
static int home_text(sqlite3* db, const sel_user_op* op, int64_t* rows) {
    uint32_t used = 0;
    sqlite3_stmt* st = NULL;
    int rc;
    long fresh = 0;
    char tagged[80];

    op->out[0] = '\0';
    *rows = 0;
    home_put(op, &used, rows, "H", "Recently played");
    rc = home_list(db, op, &used, rows,
                   "SELECT COALESCE(s.name, h.key), s.artist FROM (SELECT key, MAX(played_at) AS last FROM history GROUP BY key) h"
                   " LEFT JOIN songs s ON s.key = h.key ORDER BY h.last DESC, h.key LIMIT ?1");
    if (rc != SQLITE_OK) return rc;
    home_put(op, &used, rows, "H", "Jump back in");
    rc = home_list(db, op, &used, rows,
                   "SELECT s.album, s.artist FROM history h JOIN songs s ON s.key = h.key"
                   " WHERE h.skipped = 1 AND s.album IS NOT NULL AND s.album <> ''"
                   " AND NOT EXISTS (SELECT 1 FROM history f WHERE f.key = h.key AND f.skipped = 0)"
                   " GROUP BY s.album ORDER BY MAX(h.played_at) DESC, s.album LIMIT ?1");
    if (rc != SQLITE_OK) return rc;
    home_put(op, &used, rows, "H", "New this week");
    rc = sqlite3_prepare_v2(db, "SELECT value FROM meta WHERE key = 'modland.new_this_week'", -1, &st, NULL);
    if (rc != SQLITE_OK) return rc;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const char* v = (const char*)sqlite3_column_text(st, 0);
        fresh = v != NULL ? atol(v) : 0;
    }
    sqlite3_finalize(st);
    snprintf(tagged, sizeof tagged, "%ld %ld new songs on Modland", fresh, fresh);
    home_put(op, &used, rows, "C", tagged);
    return SQLITE_OK;
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
    } else if (op->op == SEL_USER_HOME && op->out != NULL && op->out_cap > 0) {
        rc = home_text(lib->db, op, &rows);
    } else if (op->op == SEL_USER_RATING_PENDING && op->out != NULL && op->out_cap > 0) {
        rc = rating_pending(lib->db, op, &rows);
    } else if (op->op == SEL_USER_FLAG_EXPORT && op->name != NULL) {
        rows = 0;
        rc = flags_export(lib->db, op->name, &rows);
    } else if (op->op == SEL_USER_STATS && op->out != NULL && op->out_cap > 0) {
        rows = stats_build(lib->db, op->out, op->out_cap);
        rc = SQLITE_OK;
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
        rc = sqlite3_prepare_v2(lib->db,
                                op->to ? "UPDATE songs SET loudness_gain = ?2, loudness_prov = 1 WHERE key = ?1"
                                         " AND (loudness_gain IS NULL OR loudness_prov = 1)"
                                       : "UPDATE songs SET loudness_gain = ?2, loudness_prov = 0 WHERE key = ?1",
                                -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 2, op->at);
            rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
            sqlite3_finalize(st);
        }
        if (rc == SQLITE_OK && sqlite3_changes(lib->db) == 0) {
            st = NULL;
            rc = sqlite3_prepare_v2(lib->db,
                                    op->to ? "INSERT OR IGNORE INTO songs(key, source, path, loudness_gain, loudness_prov)"
                                             " VALUES(?1, 'local', ?3, ?2, 1)"
                                           : "INSERT INTO songs(key, source, path, loudness_gain) VALUES(?1, 'local', ?3, ?2)",
                                    -1, &st, NULL);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
                sqlite3_bind_int64(st, 2, op->at);
                sqlite3_bind_text(st, 3, op->value != NULL ? op->value : op->key, -1, SQLITE_STATIC);
                rc = sqlite3_step(st) == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
                sqlite3_finalize(st);
            }
        }
    } else if (op->op == SEL_USER_RATING_SET) {
        rc = rating_set(lib->db, op);
    } else if (op->op == SEL_USER_RATING_GET) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db, "SELECT rating FROM ratings WHERE key = ?1", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            if (sqlite3_step(st) == SQLITE_ROW) {
                rows = sqlite3_column_int64(st, 0);
                found = 1;
            }
            sqlite3_finalize(st);
        }
    } else if (op->op == SEL_USER_RATING_DONE) {
        rc = user_exec(lib->db, "DELETE FROM pending_push WHERE kind = 'rating' AND key = ?1", op, 0);
        if (rc == SQLITE_OK) rc = user_exec(lib->db, "UPDATE ratings SET synced = 1 WHERE key = ?1", op, 0);
    } else if (op->op == SEL_USER_LOUDNESS_GET) {
        sqlite3_stmt* st = NULL;
        rc = sqlite3_prepare_v2(lib->db, "SELECT loudness_gain, loudness_prov FROM songs WHERE key = ?1", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL) {
                rows = sqlite3_column_int64(st, 0);
                found = sqlite3_column_int(st, 1) != 0 ? 2 : 1;
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
