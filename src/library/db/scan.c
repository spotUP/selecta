/* SEL_LIB_REQ_SCAN_BATCH (plan Phase 4, I158): writes one batch of catalogue rows into `files` and
 * `songs` inside one transaction. Runs on the worker, which owns the connection. Re-scanning a path
 * updates its rows in place (the song key is unique), so a scan is idempotent. */
#include "db_internal.h"

#include <stdio.h>
#include <string.h>

static const char SQL_FILE[] =
    "INSERT OR REPLACE INTO files(source, path, format, artist, name, md5, size) VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)";
static const char SQL_SONG[] =
    "INSERT INTO songs(key, source, path, name, artist, format, year, duration_ms, md5, size, added)"
    " VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)"
    " ON CONFLICT(key) DO UPDATE SET name=excluded.name, artist=excluded.artist, format=excluded.format,"
    " year=excluded.year, duration_ms=excluded.duration_ms, md5=excluded.md5, size=excluded.size";

static void bind_text(sqlite3_stmt* st, int i, const char* s) {
    if (s != NULL && s[0] != '\0') {
        sqlite3_bind_text(st, i, s, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(st, i);
    }
}

static void bind_positive(sqlite3_stmt* st, int i, sqlite3_int64 v) {
    if (v > 0) {
        sqlite3_bind_int64(st, i, v);
    } else {
        sqlite3_bind_null(st, i);
    }
}

void sel_library_scan_batch(sel_library* lib, const sel_scan_batch* b, sel_message* reply) {
    sqlite3_stmt* sf = NULL;
    sqlite3_stmt* ss = NULL;
    int rc;
    int written = 0;
    uint32_t i;
    char key[1200];

    rc = sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(lib->db, SQL_FILE, -1, &sf, NULL);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(lib->db, SQL_SONG, -1, &ss, NULL);
    }
    for (i = 0; rc == SQLITE_OK && i < b->count; i++) {
        const sel_scan_row* r = &b->rows[i];

        snprintf(key, sizeof key, "%s:%s", r->source, r->path);
        sqlite3_reset(sf);
        sqlite3_bind_text(sf, 1, r->source, -1, SQLITE_STATIC);
        sqlite3_bind_text(sf, 2, r->path, -1, SQLITE_STATIC);
        bind_text(sf, 3, r->format);
        bind_text(sf, 4, r->artist);
        bind_text(sf, 5, r->name);
        bind_text(sf, 6, r->md5);
        bind_positive(sf, 7, r->size);
        rc = sqlite3_step(sf);
        if (rc != SQLITE_DONE) {
            break;
        }
        sqlite3_reset(ss);
        sqlite3_bind_text(ss, 1, key, -1, SQLITE_STATIC);
        sqlite3_bind_text(ss, 2, r->source, -1, SQLITE_STATIC);
        sqlite3_bind_text(ss, 3, r->path, -1, SQLITE_STATIC);
        bind_text(ss, 4, r->name);
        bind_text(ss, 5, r->artist);
        bind_text(ss, 6, r->format);
        bind_positive(ss, 7, r->year);
        bind_positive(ss, 8, r->duration_ms);
        bind_text(ss, 9, r->md5);
        bind_positive(ss, 10, r->size);
        sqlite3_bind_int64(ss, 11, b->now);
        rc = sqlite3_step(ss);
        if (rc != SQLITE_DONE) {
            break;
        }
        rc = SQLITE_OK;
        written++;
    }
    sqlite3_finalize(sf);
    sqlite3_finalize(ss);
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(lib->db, "COMMIT", NULL, NULL, NULL);
    } else {
        sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL);
        written = 0;
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_SCAN_BATCH;
    reply->argc = 2;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = written;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
}

/* SEL_LIB_REQ_REMOVE (I161): deletes the files and songs rows of the listed paths (a changelog's
 * deleted: and moved: entries). A path that is not in the catalogue is not an error. */
void sel_library_remove(sel_library* lib, const sel_remove_batch* b, sel_message* reply) {
    sqlite3_stmt* sf = NULL;
    sqlite3_stmt* ss = NULL;
    int rc;
    int removed = 0;
    uint32_t i;
    char key[1200];

    rc = sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(lib->db, "DELETE FROM files WHERE source=?1 AND path=?2", -1, &sf, NULL);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(lib->db, "DELETE FROM songs WHERE key=?1", -1, &ss, NULL);
    }
    for (i = 0; rc == SQLITE_OK && i < b->count; i++) {
        snprintf(key, sizeof key, "%s:%s", b->source, b->paths[i]);
        sqlite3_reset(sf);
        sqlite3_bind_text(sf, 1, b->source, -1, SQLITE_STATIC);
        sqlite3_bind_text(sf, 2, b->paths[i], -1, SQLITE_STATIC);
        rc = sqlite3_step(sf);
        if (rc != SQLITE_DONE) {
            break;
        }
        sqlite3_reset(ss);
        sqlite3_bind_text(ss, 1, key, -1, SQLITE_STATIC);
        rc = sqlite3_step(ss);
        if (rc != SQLITE_DONE) {
            break;
        }
        rc = SQLITE_OK;
        removed += sqlite3_changes(lib->db);
    }
    sqlite3_finalize(sf);
    sqlite3_finalize(ss);
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(lib->db, "COMMIT", NULL, NULL, NULL);
    } else {
        sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL);
        removed = 0;
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_REMOVE;
    reply->argc = 2;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = removed;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
}

/* Tables whose `key` column names a song; a move re-keys them all with the songs row. */
static const char* const KEYED_TABLES[] = {"history", "liked",  "kept",       "ratings",         "cues",
                                           "notes",   "flags",  "stickers",   "genre_cache",     "pending_push",
                                           "playlist_entries"};

static int exec_bind2(sqlite3* db, const char* sql, const char* a, const char* b, const char* c, const char* d,
                      const char* e, int* changes) {
    sqlite3_stmt* st = NULL;
    const char* v[5];
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);
    int i;

    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
    v[4] = e;
    for (i = 0; rc == SQLITE_OK && i < 5 && v[i] != NULL; i++) {
        rc = sqlite3_bind_text(st, i + 1, v[i], -1, SQLITE_STATIC);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(st);
        rc = rc == SQLITE_DONE ? SQLITE_OK : rc;
    }
    sqlite3_finalize(st);
    if (changes != NULL) {
        *changes = rc == SQLITE_OK ? sqlite3_changes(db) : 0;
    }
    return rc;
}

/* SEL_LIB_REQ_MOVE (I161b): re-keys one song in place so everything stored under its key survives. */
void sel_library_move(sel_library* lib, const sel_move* m, sel_message* reply) {
    char oldkey[1200];
    char newkey[1200];
    char sql[160];
    int moved = 0;
    int changes = 0;
    int rc;
    size_t t;

    snprintf(oldkey, sizeof oldkey, "%s:%s", m->source, m->from);
    snprintf(newkey, sizeof newkey, "%s:%s", m->to->source, m->to->path);
    rc = sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL);
    if (rc == SQLITE_OK && strcmp(oldkey, newkey) != 0) {
        rc = exec_bind2(lib->db, "DELETE FROM songs WHERE key=?1 AND EXISTS(SELECT 1 FROM songs WHERE key=?2)", newkey,
                        oldkey, NULL, NULL, NULL, NULL);
        if (rc == SQLITE_OK) {
            rc = exec_bind2(lib->db, "DELETE FROM files WHERE source=?1 AND path=?2 AND EXISTS("
                                     "SELECT 1 FROM files WHERE source=?1 AND path=?3)",
                            m->to->source, m->to->path, m->from, NULL, NULL, NULL);
        }
    }
    if (rc == SQLITE_OK) {
        sqlite3_stmt* st = NULL;

        rc = sqlite3_prepare_v2(lib->db,
                                "UPDATE songs SET key=?1, path=?2, name=?3, artist=?4, format=?5 WHERE key=?6", -1, &st,
                                NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, newkey, -1, SQLITE_STATIC);
            sqlite3_bind_text(st, 2, m->to->path, -1, SQLITE_STATIC);
            bind_text(st, 3, m->to->name);
            bind_text(st, 4, m->to->artist);
            bind_text(st, 5, m->to->format);
            sqlite3_bind_text(st, 6, oldkey, -1, SQLITE_STATIC);
            rc = sqlite3_step(st);
            rc = rc == SQLITE_DONE ? SQLITE_OK : rc;
            moved = rc == SQLITE_OK ? sqlite3_changes(lib->db) : 0;
        }
        sqlite3_finalize(st);
    }
    if (rc == SQLITE_OK && moved > 0) {
        sqlite3_stmt* st = NULL;

        rc = sqlite3_prepare_v2(lib->db,
                                "UPDATE files SET path=?1, format=?2, artist=?3, name=?4 WHERE source=?5 AND path=?6", -1,
                                &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, m->to->path, -1, SQLITE_STATIC);
            bind_text(st, 2, m->to->format);
            bind_text(st, 3, m->to->artist);
            bind_text(st, 4, m->to->name);
            sqlite3_bind_text(st, 5, m->source, -1, SQLITE_STATIC);
            sqlite3_bind_text(st, 6, m->from, -1, SQLITE_STATIC);
            rc = sqlite3_step(st);
            rc = rc == SQLITE_DONE ? SQLITE_OK : rc;
        }
        sqlite3_finalize(st);
        for (t = 0; rc == SQLITE_OK && t < sizeof KEYED_TABLES / sizeof KEYED_TABLES[0]; t++) {
            snprintf(sql, sizeof sql, "UPDATE OR REPLACE %s SET key=?1 WHERE key=?2", KEYED_TABLES[t]);
            rc = exec_bind2(lib->db, sql, newkey, oldkey, NULL, NULL, NULL, &changes);
        }
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(lib->db, "COMMIT", NULL, NULL, NULL);
    } else {
        sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL);
        moved = 0;
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_MOVE;
    reply->argc = 2;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = moved > 0;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
}

/* SEL_LIB_REQ_ANNOTATE (I162): sets duration and md5 of existing songs and stores their stickers
 * (subtune count, lengths, STIL notes). One transaction per batch. */
void sel_library_annotate(sel_library* lib, const sel_annotate_batch* b, sel_message* reply) {
    sqlite3_stmt* su = NULL;
    sqlite3_stmt* sf = NULL;
    sqlite3_stmt* sk = NULL;
    int rc;
    int done = 0;
    uint32_t i, j;
    char key[1200];

    rc = sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(lib->db,
                                "UPDATE songs SET duration_ms=COALESCE(?2, duration_ms), md5=COALESCE(?3, md5) WHERE key=?1",
                                -1, &su, NULL);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(lib->db,
                                "UPDATE files SET md5=COALESCE(?3, md5) WHERE source=?1 AND path=?2", -1, &sf, NULL);
    }
    if (rc == SQLITE_OK) {
        rc = sqlite3_prepare_v2(lib->db, "INSERT OR REPLACE INTO stickers(key, name, value) VALUES(?1, ?2, ?3)", -1,
                                &sk, NULL);
    }
    for (i = 0; rc == SQLITE_OK && i < b->count; i++) {
        const sel_annotation* a = &b->items[i];
        int has_md5 = a->md5 != NULL && a->md5[0] != '\0';

        snprintf(key, sizeof key, "%s:%s", b->source, a->path);
        sqlite3_reset(su);
        sqlite3_bind_text(su, 1, key, -1, SQLITE_STATIC);
        bind_positive(su, 2, a->duration_ms);
        bind_text(su, 3, has_md5 ? a->md5 : NULL);
        rc = sqlite3_step(su);
        if (rc != SQLITE_DONE) {
            break;
        }
        rc = SQLITE_OK;
        if (sqlite3_changes(lib->db) == 0) {
            continue;
        }
        sqlite3_reset(sf);
        sqlite3_bind_text(sf, 1, b->source, -1, SQLITE_STATIC);
        sqlite3_bind_text(sf, 2, a->path, -1, SQLITE_STATIC);
        bind_text(sf, 3, has_md5 ? a->md5 : NULL);
        if (sqlite3_step(sf) != SQLITE_DONE) {
            rc = SQLITE_ERROR;
            break;
        }
        for (j = 0; j < a->sticker_n; j++) {
            sqlite3_reset(sk);
            sqlite3_bind_text(sk, 1, key, -1, SQLITE_STATIC);
            sqlite3_bind_text(sk, 2, a->stickers[j].name, -1, SQLITE_STATIC);
            sqlite3_bind_text(sk, 3, a->stickers[j].value, -1, SQLITE_STATIC);
            if (sqlite3_step(sk) != SQLITE_DONE) {
                rc = SQLITE_ERROR;
                break;
            }
        }
        done++;
    }
    sqlite3_finalize(su);
    sqlite3_finalize(sf);
    sqlite3_finalize(sk);
    if (rc == SQLITE_OK) {
        rc = sqlite3_exec(lib->db, "COMMIT", NULL, NULL, NULL);
    } else {
        sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL);
        done = 0;
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_ANNOTATE;
    reply->argc = 2;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = done;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
}

/* SEL_LIB_REQ_META (I161a): one row of the meta table, read or written (e.g. "modland.new_this_week"). */
void sel_library_meta(sel_library* lib, const sel_meta_op* op, sel_message* reply) {
    sqlite3_stmt* st = NULL;
    int found = 0;
    int rc;

    if (op->set) {
        rc = sqlite3_prepare_v2(lib->db, "INSERT OR REPLACE INTO meta(key, value) VALUES(?1, ?2)", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            sqlite3_bind_text(st, 2, op->value, -1, SQLITE_STATIC);
            rc = sqlite3_step(st);
            found = rc == SQLITE_DONE;
            rc = found ? SQLITE_OK : rc;
        }
    } else {
        if (op->cap > 0) {
            op->value[0] = '\0';
        }
        rc = sqlite3_prepare_v2(lib->db, "SELECT value FROM meta WHERE key=?1", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
            rc = sqlite3_step(st);
            if (rc == SQLITE_ROW) {
                const char* v = (const char*)sqlite3_column_text(st, 0);
                if (v != NULL && op->cap > 0) {
                    snprintf(op->value, op->cap, "%s", v);
                }
                found = 1;
                rc = SQLITE_OK;
            } else if (rc == SQLITE_DONE) {
                rc = SQLITE_OK;
            }
        }
    }
    sqlite3_finalize(st);
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_META;
    reply->argc = 2;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = found;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
}
