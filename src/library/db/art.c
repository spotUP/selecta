/* Album artwork access for the desktop UI (design doc 2.7, DUI-P2-4). The worker half serves SEL_LIB_REQ_ART on
 * `albums` and `artwork`; sel_library_art_provider wraps it as the sel_ui_art table the shell hands to UI plugins,
 * so plugins never see sqlite or the library headers. */
#include "db_internal.h"

#include "../song_key.h"
#include "../source.h"

#include <selecta/ui.h>

#include <string.h>

/* The album of a song: queue ref -> canonical key -> (source, path) or the stored songs.key -> songs row -> albums row with the song's album
 * name and artist (source matched when the albums row sets one). The basename of a file is never an album name. */
static int art_find(sqlite3* db, const sel_art_op* op) {
    char key[1024], source[64], path[1024];
    sqlite3_stmt* st = NULL;
    int id = 0;

    if (!sel_song_key_of(op->ref, key, sizeof key) || !sel_song_key_split(key, source, sizeof source, path, sizeof path)) return 0;
    if (sqlite3_prepare_v2(db,
                           "SELECT a.id FROM songs s JOIN albums a ON a.name = s.album AND a.artist IS s.artist"
                           " AND (a.source IS NULL OR a.source = s.source)"
                           " WHERE (s.source = ?1 AND s.path = ?2) OR s.key = ?3 ORDER BY a.id LIMIT 1",
                           -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, source, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, path, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, op->ref, -1, SQLITE_STATIC); /* a shelf tile names its song by the stored key, which need not be source:path */
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return id;
}

static int art_get(sqlite3* db, const sel_art_op* op) {
    sqlite3_stmt* st = NULL;
    int n = 0;

    if (sqlite3_prepare_v2(db, "SELECT data FROM artwork WHERE album_id = ?1 AND kind = ?2", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int64(st, 1, op->album);
    sqlite3_bind_text(st, 2, op->kind, -1, SQLITE_STATIC);
    if (sqlite3_step(st) == SQLITE_ROW) {
        n = sqlite3_column_bytes(st, 0);
        if (n > 0 && op->data != NULL) memcpy(op->data, sqlite3_column_blob(st, 0), (size_t)n < op->cap ? (size_t)n : op->cap);
    }
    sqlite3_finalize(st);
    return n;
}

static int art_put(sqlite3* db, const sel_art_op* op) {
    sqlite3_stmt* st = NULL;
    int ok = 0;

    if (sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO artwork(album_id, kind, data) VALUES(?1, ?2, ?3)", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int64(st, 1, op->album);
    sqlite3_bind_text(st, 2, op->kind, -1, SQLITE_STATIC);
    sqlite3_bind_blob(st, 3, op->data, (int)op->len, SQLITE_STATIC);
    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

/* I437: the scan found an embedded cover. The albums row is the one art_find joins on (name, artist, source local);
 * created when the scan is the first to see the album. Raw image bytes (PNG / JPEG), as the fixture rows hold them. */
static int art_store(sqlite3* db, const sel_art_op* op) {
    sqlite3_stmt* st = NULL;
    sel_art_op put = *op;
    int id = 0;

    if (op->name == NULL || op->name[0] == '\0') return 0;
    if (sqlite3_prepare_v2(db, "SELECT id FROM albums WHERE name = ?1 AND artist IS ?2 AND (source IS NULL OR source = 'local') ORDER BY id LIMIT 1",
                           -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(st, 1, op->name, -1, SQLITE_STATIC);
    if (op->artist != NULL && op->artist[0] != '\0') sqlite3_bind_text(st, 2, op->artist, -1, SQLITE_STATIC);
    else sqlite3_bind_null(st, 2);
    if (sqlite3_step(st) == SQLITE_ROW) id = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    if (id == 0) {
        if (sqlite3_prepare_v2(db, "INSERT INTO albums(name, artist, source) VALUES(?1, ?2, 'local')", -1, &st, NULL) != SQLITE_OK) return 0;
        sqlite3_bind_text(st, 1, op->name, -1, SQLITE_STATIC);
        if (op->artist != NULL && op->artist[0] != '\0') sqlite3_bind_text(st, 2, op->artist, -1, SQLITE_STATIC);
        else sqlite3_bind_null(st, 2);
        if (sqlite3_step(st) == SQLITE_DONE) id = (int)sqlite3_last_insert_rowid(db);
        sqlite3_finalize(st);
        if (id == 0) return 0;
    }
    put.album = id;
    return art_put(db, &put);
}

void sel_library_art_op(sel_library* lib, const sel_art_op* op, sel_message* reply) {
    int v = 0;

    if (op->op == SEL_ART_FIND && op->ref != NULL) {
        v = art_find(lib->db, op);
    } else if (op->op == SEL_ART_GET && op->kind != NULL) {
        v = art_get(lib->db, op);
    } else if (op->op == SEL_ART_PUT && op->kind != NULL && op->data != NULL) {
        v = art_put(lib->db, op);
    } else if (op->op == SEL_ART_STORE && op->kind != NULL && op->data != NULL) {
        v = art_store(lib->db, op);
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_ART;
    reply->argc = 1;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = v;
}

static int art_call(sel_library* lib, const sel_art_op* op) {
    sel_message reply;

    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(lib, SEL_LIB_REQ_ART, op, &reply) || reply.argc < 1) return 0;
    return reply.args[0].v.i;
}

static int64_t prov_find(void* ctx, const char* song_ref) {
    sel_art_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_FIND;
    op.ref = song_ref;
    return art_call((sel_library*)ctx, &op);
}

static uint32_t prov_get(void* ctx, int64_t album, const char* kind, uint8_t* out, uint32_t cap) {
    sel_art_op op;
    int n;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_GET;
    op.album = album;
    op.kind = kind;
    op.data = out;
    op.cap = cap;
    n = art_call((sel_library*)ctx, &op);
    return n > 0 ? (uint32_t)n : 0u;
}

static int prov_put(void* ctx, int64_t album, const char* kind, const uint8_t* data, uint32_t len) {
    sel_art_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_PUT;
    op.album = album;
    op.kind = kind;
    op.data = (uint8_t*)data;
    op.len = len;
    return art_call((sel_library*)ctx, &op);
}

void sel_library_art_provider(sel_library* lib, sel_ui_art* out) {
    out->ctx = lib;
    out->find = prov_find;
    out->get = prov_get;
    out->put = prov_put;
}
