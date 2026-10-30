/* I782: the draw path must never wait on the library worker. A library request of a thread in nowait mode returns 0 at once while
 * another caller (the sync job) owns the library, counts itself in sel_engine_lib_deferred, and works again when the library is
 * free. sel_engine_song_briefs fetches title, artist, album and album id of many songs in ONE request (the playlist page used to
 * ask two blocked round trips per song). Fixture: 2 songs and one unknown key. */
#include <pthread.h>
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <selecta/registry.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static volatile int g_held, g_release;

/* Plays the sync job: owns the library call lock until released. */
static void* holder(void* arg) {
    sel_library* lib = arg;

    pthread_mutex_lock(&lib->call_lock);
    g_held = 1;
    while (!g_release) usleep(1000);
    pthread_mutex_unlock(&lib->call_lock);
    return NULL;
}

static long ms_since(const struct timespec* t0) {
    struct timespec t1;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (long)((t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_nsec - t0->tv_nsec) / 1000000L);
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_nowait_XXXXXX";
    char db[256], out[512];
    const char keys[] = "local:a.mod\0local:zz.mod\0local:c.mod\0";
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    pthread_t th;
    struct timespec t0;
    uint32_t done = 0, d0;
    const char* p;
    int was;
    sel_engine_stats s;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    CHECK(sqlite3_exec(lib->db,
                       "INSERT INTO songs(key, source, path, name, artist, album, added) VALUES"
                       "('local:a.mod','local','a.mod','Alpha','Ann','Alb1',100),('local:c.mod','local','c.mod','Gamma','Bob',NULL,300);"
                       "INSERT INTO albums(name, artist) VALUES('Alb1','Ann');",
                       NULL, NULL, NULL) == SQLITE_OK);

    /* One request, three keys: Alpha with album id, an unknown key as four empty strings, Gamma without album. */
    CHECK(sel_engine_song_briefs(e, keys, out, sizeof out, &done) == 1);
    CHECK(done == 3u);
    p = out;
    CHECK(strcmp(p, "Alpha") == 0);
    p += strlen(p) + 1;
    CHECK(strcmp(p, "Ann") == 0);
    p += strlen(p) + 1;
    CHECK(strcmp(p, "Alb1") == 0);
    p += strlen(p) + 1;
    CHECK(atoi(p) > 0);
    p += strlen(p) + 1;
    CHECK(p[0] == '\0' && p[1] == '\0' && p[2] == '\0' && p[3] == '\0'); /* the unknown key */
    p += 4;
    CHECK(strcmp(p, "Gamma") == 0);

    /* The sync owns the library: a nowait request is refused at once, a normal one would wait for the release. */
    pthread_create(&th, NULL, holder, lib);
    while (!g_held) usleep(1000);
    d0 = sel_engine_lib_deferred();
    was = sel_engine_lib_nowait(1);
    CHECK(was == 0);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    CHECK(sel_engine_song_briefs(e, keys, out, sizeof out, &done) == 0);
    CHECK(ms_since(&t0) < 100); /* did not wait on the held lock */
    CHECK(sel_engine_lib_deferred() == d0 + 1u);
    sel_engine_stats_get(e, &s); /* the per frame stats read asks for rating_pending under nowait: refused, cached value reported */
    CHECK(s.rating_pending == 0u);
    CHECK(sel_engine_lib_deferred() >= d0 + 2u);
    sel_engine_lib_nowait(was);
    g_release = 1;
    pthread_join(th, NULL);

    /* Free again: the same nowait call is served. */
    sel_engine_lib_nowait(1);
    d0 = sel_engine_lib_deferred();
    CHECK(sel_engine_song_briefs(e, keys, out, sizeof out, &done) == 1 && done == 3u);
    CHECK(sel_engine_lib_deferred() == d0);
    sel_engine_lib_nowait(0);

    /* Majority format of a big source: the cache answers like the scan, costs no scan, and follows a change of the database. */
    {
        char raw[16], c1[16], c2[16], c3[16];
        long scan_ms, hit_ms;

        CHECK(sqlite3_exec(lib->db,
                           "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<60000)"
                           "INSERT INTO songs(key, source, path, name, artist, added) SELECT 'big:'||i,'big','d/'||i||'.mod','s'||i,'X',1 FROM n;",
                           NULL, NULL, NULL) == SQLITE_OK);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_db_majority_format(lib->db, SEL_LIB_ITEM_SOURCE, "big", raw, sizeof raw);
        scan_ms = ms_since(&t0);
        sel_db_majority_format_cached(lib, SEL_LIB_ITEM_SOURCE, "big", c1, sizeof c1);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sel_db_majority_format_cached(lib, SEL_LIB_ITEM_SOURCE, "big", c2, sizeof c2);
        hit_ms = ms_since(&t0);
        printf("majority_format 60000 songs: scan %ld ms, cached hit %ld ms\n", scan_ms, hit_ms);
        CHECK(strcmp(raw, c1) == 0 && strcmp(c1, c2) == 0 && raw[0] != '\0');
        CHECK(hit_ms <= 1);
        CHECK(sqlite3_exec(lib->db,
                           "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<70000)"
                           "INSERT INTO songs(key, source, path, name, artist, added) SELECT 'big2:'||i,'big','e/'||i||'.sid','t'||i,'X',1 FROM n;",
                           NULL, NULL, NULL) == SQLITE_OK);
        sel_db_majority_format(lib->db, SEL_LIB_ITEM_SOURCE, "big", raw, sizeof raw);
        sel_db_majority_format_cached(lib, SEL_LIB_ITEM_SOURCE, "big", c3, sizeof c3);
        CHECK(strcmp(raw, c3) == 0 && strcmp(c1, c3) != 0); /* the change flipped the majority; the cache followed */
    }

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail == 0) printf("library_nowait_ok\n");
    return g_fail != 0;
}
