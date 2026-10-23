/* User data writes and reads (plan 2026-10-25-m3-library.md Phase 6, I170): finished plays and skips in
 * `history`, LIKE and KEEP in `liked` and `kept`, per-song replayer options as the sticker "rp.config".
 * One request, SEL_LIB_REQ_USER, runs on the worker like every other library request. */
#include "db_internal.h"

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

void sel_library_user(sel_library* lib, const sel_user_op* op, sel_message* reply) {
    int rc = SQLITE_MISUSE;
    int found = 0;
    int64_t rows = -1;

    if (op->op == SEL_USER_FLAG_EXPORT && op->name != NULL) {
        rows = 0;
        rc = flags_export(lib->db, op->name, &rows);
    } else if (op->key == NULL || op->key[0] == '\0') {
        /* rc stays SQLITE_MISUSE */
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
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_USER;
    reply->argc = 3;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = rc == SQLITE_OK ? 1 + found : 0;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
    reply->args[2].type = SEL_ARG_INT; /* FLAG: rows in `flags`; FLAG_EXPORT: lines written */
    reply->args[2].v.i = (int32_t)rows;
}
