/* I1231b: the search result cache where it lives (src/library/db/search_cache.c): query normalisation, the 7-day TTL,
 * the least-recently-used cap, the per-source drop, the drop when a source's rows go (source reset or removal) and the
 * library clear. */
#include <selecta/host.h>
#include <selecta/machine.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "../engine/engine_test_db_lock.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fail;

#define CHECK(c)                                          \
    do {                                                  \
        if (!(c)) {                                       \
            printf("FAIL line %d: %s\n", __LINE__, #c);   \
            g_fail++;                                     \
        }                                                 \
    } while (0)

static void sql(sel_library* lib, const char* text) {
    char* err = NULL;
    int rc;
    db_enter(lib);
    rc = sqlite3_exec(lib->db, text, NULL, NULL, &err);
    db_leave(lib);
    if (rc != SQLITE_OK) {
        printf("FAIL sql: %s\n", err != NULL ? err : "?");
        g_fail++;
    }
}

static int get(sel_library* lib, const char* src, const char* q, int64_t now, char* out) {
    uint32_t len = 0;
    int ok = sel_library_search_cache_get(lib, src, q, now, out, 64, &len);
    out[ok ? len : 0] = '\0';
    return ok;
}

static int put_cap(sel_library* lib, const char* src, const char* q, int64_t now, uint32_t cap_rows) {
    sel_searchcache_op op;
    sel_message reply;

    memset(&op, 0, sizeof op);
    op.op = SEL_SEARCH_CACHE_PUT;
    op.source = src;
    op.query = q;
    op.now = now;
    op.data = (void*)q;
    op.len = (uint32_t)strlen(q);
    op.cap_rows = cap_rows;
    return sel_library_request(lib, SEL_LIB_REQ_SEARCH_CACHE, &op, &reply) && reply.args[0].v.i == 1;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char path[] = "/tmp/sel-i1231bu-XXXXXX";
    char db[128], buf[80], norm[64];
    const int64_t t = 1800000000;
    sel_library* lib;
    int fd = mkstemp(path);

    if (fd < 0) {
        puts("FAIL mkstemp");
        return 1;
    }
    close(fd);
    snprintf(db, sizeof db, "%s.db", path);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    if (lib == NULL) {
        puts("FAIL open");
        return 1;
    }

    sel_library_search_cache_norm("  Jam\t  SESSION ", norm, sizeof norm);
    CHECK(strcmp(norm, "jam session") == 0);

    /* TTL: served up to 7 days, not after. A cache write is no library change: write_gen stays, else the shown Search re-runs
     * (search_refresh) and stores again in a loop. */
    {
        uint32_t g0 = sel_library_write_gen(lib);
        CHECK(sel_library_search_cache_put(lib, "a", "Jam", t, "hits-a", 6));
        CHECK(sel_library_write_gen(lib) == g0);
    }
    CHECK(get(lib, "a", " jam ", t + 10, buf) && strcmp(buf, "hits-a") == 0);
    CHECK(get(lib, "a", "jam", t + SEL_SEARCH_CACHE_TTL_S - 1, buf));
    CHECK(!get(lib, "a", "jam", t + SEL_SEARCH_CACHE_TTL_S, buf));
    CHECK(!get(lib, "b", "jam", t, buf)); /* per source */
    CHECK(SEL_SEARCH_CACHE_TTL_S == 7 * 24 * 3600 && SEL_SEARCH_CACHE_CAP == 500u);

    /* cap: past 3 rows the least recently used goes; a GET counts as a use */
    sql(lib, "DELETE FROM search_cache");
    CHECK(put_cap(lib, "a", "q1", t, 3));
    CHECK(put_cap(lib, "a", "q2", t, 3));
    CHECK(put_cap(lib, "a", "q3", t, 3));
    CHECK(get(lib, "a", "q1", t, buf)); /* q1 now most recent, q2 least */
    CHECK(put_cap(lib, "a", "q4", t, 3));
    CHECK(get(lib, "a", "q1", t, buf) && get(lib, "a", "q3", t, buf) && get(lib, "a", "q4", t, buf));
    CHECK(!get(lib, "a", "q2", t, buf));

    /* drop by source list (switched-off sources) */
    CHECK(sel_library_search_cache_put(lib, "b", "q1", t, "x", 1));
    CHECK(sel_library_search_cache_put(lib, "c", "q1", t, "x", 1));
    CHECK(sel_library_search_cache_drop(lib, "zz,b,"));
    CHECK(!get(lib, "b", "q1", t, buf) && get(lib, "c", "q1", t, buf) && get(lib, "a", "q1", t, buf));

    /* a source whose rows all go (reset or removal) loses its cached searches */
    sql(lib, "INSERT INTO songs(key, source, path, name, artist, added) VALUES('c:x.mod','c','x.mod','X','Y',1)");
    sql(lib, "DELETE FROM songs WHERE source='c'");
    CHECK(!get(lib, "c", "q1", t, buf) && get(lib, "a", "q1", t, buf));

    /* drop everything */
    CHECK(sel_library_search_cache_drop(lib, NULL));
    CHECK(!get(lib, "a", "q1", t, buf));

    sel_library_close(lib);
    unlink(path);
    unlink(db);
    if (g_fail != 0) {
        return 1;
    }
    puts("search_cache_unit_ok");
    return 0;
}
