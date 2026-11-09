/* I1231b: the live search result cache (table search_cache, schema v19). The engine shows a cached answer of a live source at
 * once and refreshes it from the network behind it. Defaults live in db.h: SEL_SEARCH_CACHE_TTL_S (7 days, an older entry is
 * never served) and SEL_SEARCH_CACHE_CAP (500 rows, least recently used go first on every PUT). */
#include "db_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void sel_library_search_cache_norm(const char* query, char* out, size_t cap) {
    size_t n = 0;
    int space = 0;

    if (cap == 0u) return;
    for (; query != NULL && *query != '\0'; query++) {
        unsigned char c = (unsigned char)*query;

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            space = n > 0u;
            continue;
        }
        if (space && n + 1u < cap) out[n++] = ' ';
        space = 0;
        if (n + 1u < cap) out[n++] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    }
    out[n] = '\0';
}

/* One statement: ?1 and ?2 text (NULL = unbound), ?3 an int, ?4 a blob (NULL = unbound), ?5 an int. */
static int cache_exec(sel_library* lib, const char* sql, const char* a, const char* b, int64_t i3, const void* blob, uint32_t len,
                      int64_t i5) {
    sqlite3_stmt* st = NULL;
    int rc = sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL);

    if (rc == SQLITE_OK) {
        if (a != NULL) sqlite3_bind_text(st, 1, a, -1, SQLITE_STATIC);
        if (b != NULL) sqlite3_bind_text(st, 2, b, -1, SQLITE_STATIC);
        sqlite3_bind_int64(st, 3, i3);
        if (blob != NULL) sqlite3_bind_blob(st, 4, blob, (int)len, SQLITE_STATIC);
        sqlite3_bind_int64(st, 5, i5);
        rc = sqlite3_step(st);
        rc = rc == SQLITE_DONE || rc == SQLITE_ROW ? SQLITE_OK : rc;
    }
    sqlite3_finalize(st);
    return rc;
}

/* SEL_LIB_REQ_SEARCH_CACHE, see sel_searchcache_op. */
void sel_library_search_cache(sel_library* lib, const sel_searchcache_op* op, sel_message* reply) {
    sel_searchcache_op* w = (sel_searchcache_op*)op;
    char q[512];
    int64_t now = op->now != 0 ? op->now : (int64_t)time(NULL);
    int found = 0;
    int rc = SQLITE_OK;

    sel_library_search_cache_norm(op->query, q, sizeof q);
    if (op->op == SEL_SEARCH_CACHE_GET) {
        sqlite3_stmt* st = NULL;

        rc = sqlite3_prepare_v2(lib->db, "SELECT hits FROM search_cache WHERE source=?1 AND query=?2 AND at>?3", -1, &st, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(st, 1, op->source, -1, SQLITE_STATIC);
            sqlite3_bind_text(st, 2, q, -1, SQLITE_STATIC);
            sqlite3_bind_int64(st, 3, now - SEL_SEARCH_CACHE_TTL_S);
            rc = sqlite3_step(st);
            if (rc == SQLITE_ROW) {
                const void* v = sqlite3_column_blob(st, 0);
                uint32_t n = (uint32_t)sqlite3_column_bytes(st, 0);

                w->len = n < op->cap ? n : op->cap;
                if (op->data != NULL && v != NULL) memcpy(op->data, v, w->len);
                found = 1;
            }
            rc = rc == SQLITE_ROW || rc == SQLITE_DONE ? SQLITE_OK : rc;
        }
        sqlite3_finalize(st);
        if (found) { /* a served entry becomes the most recently used */
            rc = cache_exec(lib, "UPDATE search_cache SET used=(SELECT coalesce(max(used),0)+1 FROM search_cache)"
                                 " WHERE source=?1 AND query=?2 AND ?3=?3 AND ?5=?5",
                            op->source, q, 0, NULL, 0u, 0);
        }
    } else if (op->op == SEL_SEARCH_CACHE_PUT) {
        int64_t cap = op->cap_rows != 0u ? (int64_t)op->cap_rows : (int64_t)SEL_SEARCH_CACHE_CAP;

        rc = cache_exec(lib, "INSERT OR REPLACE INTO search_cache(source, query, at, used, hits) VALUES(?1, ?2, ?3,"
                             " (SELECT coalesce(max(used),0)+1 FROM search_cache), ?4) /* ?5 */",
                        op->source, q, now, op->data != NULL ? op->data : (const void*)"", op->len, 0);
        if (rc == SQLITE_OK) { /* past the cap the least recently used rows go */
            rc = cache_exec(lib, "DELETE FROM search_cache WHERE ?3=?3 AND used <= (SELECT used FROM search_cache"
                                 " ORDER BY used DESC LIMIT 1 OFFSET ?5)",
                            NULL, NULL, 0, NULL, 0u, cap);
        }
        found = rc == SQLITE_OK;
    } else if (op->op == SEL_SEARCH_CACHE_DROP) {
        if (op->source == NULL) {
            rc = sqlite3_exec(lib->db, "DELETE FROM search_cache", NULL, NULL, NULL);
        } else {
            rc = cache_exec(lib, "DELETE FROM search_cache WHERE instr(',' || ?1 || ',', ',' || source || ',') > 0 AND ?3=?3 AND ?5=?5",
                            op->source, NULL, 0, NULL, 0u, 0);
        }
        found = rc == SQLITE_OK;
    }
    memset(reply, 0, sizeof *reply);
    reply->kind = SEL_MSG_EVT;
    reply->id = SEL_LIB_REQ_SEARCH_CACHE;
    reply->argc = 2;
    reply->args[0].type = SEL_ARG_INT;
    reply->args[0].v.i = found;
    reply->args[1].type = SEL_ARG_INT;
    reply->args[1].v.i = rc;
}

static int cache_call(sel_library* lib, sel_searchcache_op* op) {
    sel_message reply;

    if (lib == NULL) return 0;
    memset(&reply, 0, sizeof reply);
    return sel_library_request(lib, SEL_LIB_REQ_SEARCH_CACHE, op, &reply) && reply.args[1].v.i == 0 && reply.args[0].v.i != 0;
}

int sel_library_search_cache_get(sel_library* lib, const char* source, const char* query, int64_t now, void* buf, uint32_t cap,
                                 uint32_t* len) {
    sel_searchcache_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_SEARCH_CACHE_GET;
    op.source = source;
    op.query = query;
    op.now = now;
    op.data = buf;
    op.cap = cap;
    if (!cache_call(lib, &op)) return 0;
    if (len != NULL) *len = op.len;
    return 1;
}

int sel_library_search_cache_put(sel_library* lib, const char* source, const char* query, int64_t now, const void* data, uint32_t len) {
    sel_searchcache_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_SEARCH_CACHE_PUT;
    op.source = source;
    op.query = query;
    op.now = now;
    op.data = (void*)data;
    op.len = len;
    return cache_call(lib, &op);
}

int sel_library_search_cache_drop(sel_library* lib, const char* sources_csv) {
    sel_searchcache_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_SEARCH_CACHE_DROP;
    op.source = sources_csv;
    return cache_call(lib, &op);
}
