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
