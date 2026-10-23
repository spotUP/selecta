/* Genre lookup (plan 2026-10-25-m3-library.md I181, T85; design doc 9.2 genre key). The worker half runs
 * SEL_LIB_REQ_GENRE on `genre_cache` and `songs.genre`; the caller half (sel_library_genre_sync) reads the
 * files, builds the UADE key and asks the DEViLBOX genre endpoint through the one HTTP client when the cache
 * has no entry. Endpoint: research 2026-10-25-m3-external-endpoints.md (server routes/genre.ts). */
#include "db_internal.h"

#include "../net/http.h"
#include "../net/mirrors.h"
#include "../xxh32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NAME_ENC_MAX 800u
#define URL_MAX 1100u
#define GENRE_MAX 128u
#define REPLY_MAX 1024u

/* ---- worker half ---- */

static int op_next(sqlite3* db, const sel_genre_op* op, int64_t* id) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, "SELECT id, key, path FROM songs WHERE source = 'local' AND id > ?1 ORDER BY id LIMIT 1",
                                -1, &st, NULL);

    *id = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int64(st, 1, op->after);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const char* key = (const char*)sqlite3_column_text(st, 1);
        const char* path = (const char*)sqlite3_column_text(st, 2);
        size_t kl = strlen(key), pl = strlen(path);

        if (kl + pl + 2u <= op->out_cap) {
            memcpy(op->out, key, kl + 1u);
            memcpy(op->out + kl + 1u, path, pl + 1u);
            *id = sqlite3_column_int64(st, 0);
        }
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int op_get(sqlite3* db, const sel_genre_op* op, int* found) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, "SELECT genre FROM genre_cache WHERE key = ?1", -1, &st, NULL);

    *found = 0;
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const char* g = (const char*)sqlite3_column_text(st, 0);

        snprintf(op->out, op->out_cap, "%s", g != NULL ? g : "");
        *found = 1;
        rc = SQLITE_DONE;
    }
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

static int op_exec(sqlite3* db, const char* sql, const sel_genre_op* op, int put) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &st, NULL);

    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(st, 1, op->key, -1, SQLITE_STATIC);
    if (op->value != NULL && op->value[0] != '\0') {
        sqlite3_bind_text(st, 2, op->value, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(st, 2);
    }
    if (put) sqlite3_bind_int64(st, 3, op->at);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

void sel_library_genre_op(sel_library* lib, const sel_genre_op* op, sel_message* reply) {
    int rc = SQLITE_MISUSE;
    int found = 0;
    int64_t id = 0;

    if (op->op == SEL_GENRE_NEXT && op->out != NULL) {
        rc = op_next(lib->db, op, &id);
        found = id != 0;
    } else if (op->op == SEL_GENRE_GET && op->out != NULL && op->out_cap > 0) {
        rc = op_get(lib->db, op, &found);
    } else if (op->op == SEL_GENRE_PUT) {
        rc = op_exec(lib->db, "INSERT OR REPLACE INTO genre_cache(key, genre, fetched) VALUES(?1, ?2, ?3)", op, 1);
        found = rc == SQLITE_OK;
    } else if (op->op == SEL_GENRE_APPLY) {
        rc = op_exec(lib->db,
                     "UPDATE songs SET genre = ?2 WHERE key = ?1 AND NOT EXISTS"
                     " (SELECT 1 FROM stickers WHERE key = ?1 AND name = 'tag.genre')",
                     op, 0);
        found = rc == SQLITE_OK && sqlite3_changes(lib->db) > 0;
    } else if (op->op >= SEL_SONGDB_GET && op->op <= SEL_COMPOSER_NEXT) {
        rc = sel_library_songdb_op(lib, op, &found, &id);
    } else if (op->op == SEL_SIMILAR_NEXT || op->op == SEL_ALSOBY_NEXT) {
        rc = sel_library_similar_op(lib, op, &found, &id);
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_GENRE;
    reply->argc = 3;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = rc == SQLITE_OK && found;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
    reply->args[2].type = SEL_ARG_INT;
    reply->args[2].v.i = (int32_t)id;
}

/* ---- caller half ---- */

int sel_genre_call(sel_library* lib, const sel_genre_op* op, int64_t* id) {
    sel_message reply;

    memset(&reply, 0, sizeof reply);
    if (!sel_library_request(lib, SEL_LIB_REQ_GENRE, op, &reply) || reply.argc < 3 || reply.args[1].v.i != 0) return -1;
    if (id != NULL) *id = reply.args[2].v.i;
    return reply.args[0].v.i;
}

typedef struct body {
    char* text;
    size_t cap;
    size_t len;
} body;

static int body_sink(void* ctx, const void* data, size_t len) {
    body* b = ctx;

    if (b->len + len >= b->cap) return 0;
    memcpy(b->text + b->len, data, len);
    b->len += len;
    b->text[b->len] = '\0';
    return 1;
}

/* GET `url` through the one HTTP client into `text` (NUL-terminated, at most cap-1 bytes). Adds the transport
 * requests made to *requests. 1 when the answer is a 200 that fit; 0 for any other status, a transport failure or
 * an oversize body (nothing to cache). Shared by the genre and songdb lookups. */
int sel_library_http_json(const char* url, char* text, size_t cap, uint32_t* requests) {
    body b;
    sel_http_req req;
    sel_http_res res;
    sel_http_err err;

    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    b.text = text;
    b.cap = cap;
    b.len = 0;
    text[0] = '\0';
    req.url = url;
    err = sel_http_request(&req, body_sink, &b, &res);
    if (requests != NULL) *requests += res.requests;
    return err == SEL_HTTP_OK && res.status == 200;
}

/* Copies the JSON string value of `"name":` into `out` (empty when absent or not a string). Handles \" \\ \/. */
int sel_json_string(const char* json, const char* name, char* out, size_t cap) {
    char pat[32];
    const char* p;
    size_t n = 0;

    out[0] = '\0';
    snprintf(pat, sizeof pat, "\"%s\"", name);
    p = strstr(json, pat);
    if (p == NULL) return 0;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return 0;
    for (p++; *p != '\0' && *p != '"'; p++) {
        char c = *p;

        if (c == '\\' && p[1] != '\0') c = *++p;
        if (n + 1 < cap) out[n++] = c;
    }
    out[n] = '\0';
    return *p == '"';
}

static void encode_name(const char* name, char* out, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = 0;

    for (; *name != '\0' && n + 4 < cap; name++) {
        unsigned char c = (unsigned char)*name;

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') {
            out[n++] = (char)c;
        } else {
            out[n++] = '%';
            out[n++] = hex[c >> 4];
            out[n++] = hex[c & 15u];
        }
    }
    out[n] = '\0';
}

/* Reads the head of `path`; returns the full length, or -1 when unreadable. */
static int64_t read_head(const char* path, uint8_t* buf, size_t cap, size_t* got) {
    FILE* f = fopen(path, "rb");
    long size;

    if (f == NULL) return -1;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    *got = fread(buf, 1, cap, f);
    fclose(f);
    return size;
}

static const char* base_name(const char* path) {
    const char* s = path;
    const char* p;

    for (p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == ':' || *p == '\\') s = p + 1;
    }
    return s;
}

/* Asks the endpoint. Returns 1 when the reply is final (genre in `genre`, "" = no genre), 0 when the genre stays unknown. */
static int lookup(const char* key, const char* path, int64_t size, char* genre, sel_genre_stats* st) {
    char enc[NAME_ENC_MAX];
    char url[URL_MAX];
    char source[24];
    char text[REPLY_MAX];

    encode_name(base_name(path), enc, sizeof enc);
    snprintf(url, sizeof url, "%s/api/genre/%s?name=%s&size=%lld", sel_devilbox_base(), key, enc, (long long)size);
    st->genre_lookups++;
    genre[0] = '\0';
    if (!sel_library_http_json(url, text, sizeof text, &st->http_requests)) {
        return 0; /* 503 budget/queue, 502, 404, transport: unknown, not cached */
    }
    if (sel_json_string(text, "source", source, sizeof source) && strcmp(source, "modarchive") == 0) {
        sel_json_string(text, "genre", genre, GENRE_MAX);
    }
    return 1;
}

int sel_library_genre_sync(sel_library* lib, sel_genre_stats* stats) {
    static uint8_t head[SEL_UADE_KEY_HASH_BYTES];
    sel_genre_stats local;
    char buf[1400];
    int64_t after = 0, id = 0;

    if (stats == NULL) stats = &local;
    memset(stats, 0, sizeof *stats);
    if (lib == NULL) return 0;
    for (;;) {
        sel_genre_op op;
        const char* song;
        const char* path;
        char ukey[SEL_UADE_KEY_CHARS + 1u];
        char genre[GENRE_MAX];
        size_t got = 0;
        int64_t size;
        int cached;

        memset(&op, 0, sizeof op);
        op.op = SEL_GENRE_NEXT;
        op.after = after;
        op.out = buf;
        op.out_cap = sizeof buf;
        if (sel_genre_call(lib, &op, &id) < 0) return 0;
        if (id == 0) break;
        after = id;
        song = buf;
        path = buf + strlen(buf) + 1u;
        stats->songs++;
        size = read_head(path, head, sizeof head, &got);
        if (size < 0) {
            stats->unreadable++;
            continue;
        }
        sel_uade_key(head, got, (uint64_t)size, ukey);
        if (!sel_library_songdb_resolve(lib, ukey, song, stats)) return 0;
        memset(&op, 0, sizeof op);
        op.op = SEL_GENRE_GET;
        op.key = ukey;
        op.out = genre;
        op.out_cap = sizeof genre;
        cached = sel_genre_call(lib, &op, NULL);
        if (cached < 0) return 0;
        if (cached == 1) {
            stats->genre_cache_hits++;
        } else if (!lookup(ukey, path, size, genre, stats)) {
            stats->unknown++;
            continue;
        } else {
            memset(&op, 0, sizeof op);
            op.op = SEL_GENRE_PUT;
            op.key = ukey;
            op.value = genre;
            op.at = (int64_t)time(NULL);
            if (sel_genre_call(lib, &op, NULL) < 0) return 0;
        }
        if (genre[0] == '\0') {
            stats->unknown++;
            continue;
        }
        memset(&op, 0, sizeof op);
        op.op = SEL_GENRE_APPLY;
        op.key = song;
        op.value = genre;
        if (sel_genre_call(lib, &op, NULL) == 1) stats->genre_set++;
    }
    return 1;
}
