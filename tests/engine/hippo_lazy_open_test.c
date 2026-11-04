/* I1058d / I1058e: the Playlists view of HippoPlayer through the real engine entry point (sel_engine_send_line + sel_engine_pump)
 * against the net fixture (hippoplayer.se mapped to it).
 *   1. VIEW_SOURCE folders queues the PLAYLISTS_ONLY sync; its landing reloads the folder rows (sentinel: the fixture saw the list
 *      request, the library has the lists, lib_reads_applied counts the folders reads) and no prg was requested.
 *   2. Opening an unfetched list through the posted read (sel_engine_playlist_keys_post / _take) makes exactly 1 prg request, fills the
 *      list, and the pump's take posts a folders reload (lib_reads_applied advances by one more read).
 *   3. Opening the same list again makes no further request.
 * The source's home is https://hippoplayer.se/, so the sync goes over the fixture's TLS port (certificate names hippoplayer.se).
 * argv[1] = fixture work dir (tls_port, requests.jsonl), argv[2] = the fixture CA file. */
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
#include "engine_test_db_lock.h"

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void sink(void* ctx, const sel_message* evt) {
    (void)ctx;
    if (evt->id == SEL_EVT_ERROR) printf("engine error: %s %s\n", evt->argc > 0 ? evt->args[0].v.s : "", evt->argc > 1 ? evt->args[1].v.s : "");
}

/* Lines of the fixture request log that contain `needle`. */
static unsigned log_count(const char* path, const char* needle) {
    char line[4096];
    unsigned n = 0;
    FILE* f = fopen(path, "r");

    if (f == NULL) return 0;
    while (fgets(line, sizeof line, f) != NULL) {
        if (strstr(line, needle) != NULL) n++;
    }
    fclose(f);
    return n;
}

static long ms_since(const struct timespec* t0) {
    struct timespec t1;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (long)((t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_nsec - t0->tv_nsec) / 1000000L);
}

/* Pumps until the engine applied `want` posted reads since `base` (or 15 s). */
static uint32_t pump_reads(sel_engine* e, uint32_t base, uint32_t want) {
    struct timespec t0;
    sel_engine_stats s;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        sel_engine_pump(e, sink, NULL);
        sel_engine_stats_get(e, &s);
        if (s.lib_reads_applied >= base + want || ms_since(&t0) > 15000) return s.lib_reads_applied - base;
        usleep(2000);
    }
}

static uint32_t reads_now(sel_engine* e) {
    sel_engine_stats s;

    sel_engine_stats_get(e, &s);
    return s.lib_reads_applied;
}

int main(int argc, char** argv) {
    const sel_host* host;
    char dir[] = "/tmp/selecta_hippolazy_XXXXXX";
    char db[256], logp[512], portp[512], port[16] = "", map[512], pool[1024], keys[4096];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sqlite3_stmt* st;
    uint32_t id = 0, got_id = 0, n = 0, base, taken = 0;
    int ok = 0;
    struct timespec t0;
    FILE* f;

    if (argc < 3 || mkdtemp(dir) == NULL) return 2;
    snprintf(portp, sizeof portp, "%s/tls_port", argv[1]);
    snprintf(logp, sizeof logp, "%s/requests.jsonl", argv[1]);
    f = fopen(portp, "r");
    if (f == NULL || fgets(port, sizeof port, f) == NULL) return 2;
    fclose(f);
    port[strcspn(port, "\r\n")] = '\0';
    f = fopen(logp, "w"); /* earlier tests' requests are not this test's: the server appends, so truncating is safe */
    if (f != NULL) fclose(f);
    snprintf(map, sizeof map, "hippoplayer.se=http://127.0.0.1:%s", port);
    setenv("SELECTA_CA_FILE", argv[2], 1);
    setenv("SELECTA_NET_ALLOW", "127.0.0.1", 1);
    setenv("SELECTA_NET_MAP", map, 1);
    setenv("SELECTA_CACHE_DIR", dir, 1);

    host = sel_host_default();
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

    /* 1. Opening the Playlists view queues the sync; its landing reloads the folder rows. */
    base = reads_now(e);
    CHECK(sel_engine_send_line(e, "VIEW_SOURCE folders", pool, sizeof pool) == 0);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (log_count(logp, "api/v1/playlists") == 0u && ms_since(&t0) < 15000) { /* the sync runs on its own thread */
        sel_engine_pump(e, sink, NULL);
        usleep(2000);
    }
    CHECK(pump_reads(e, base, 2) >= 2u); /* the view's own folders read, then the sync's reload */
    CHECK(log_count(logp, "api/v1/playlists") >= 1u);
    CHECK(log_count(logp, ".prg") == 0u);
    db_enter(lib);
    CHECK(sqlite3_prepare_v2(lib->db, "SELECT id FROM playlists WHERE name='LMan - SID Chip Club'", -1, &st, NULL) == SQLITE_OK);
    if (sqlite3_step(st) == SQLITE_ROW) id = (uint32_t)sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    db_leave(lib);
    CHECK(id != 0u);
    CHECK(sel_library_playlist_keys(lib, id, keys, sizeof keys, &n) == SEL_FOLDER_OK && n == 0u); /* listed, not filled */

    /* 2. The posted read fills the list on its worker thread: one prg request, then the pump reloads the folder rows. */
    base = reads_now(e);
    CHECK(sel_engine_playlist_keys_post(e, id, sizeof keys) == 1);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (!taken && ms_since(&t0) < 15000) {
        sel_engine_pump(e, sink, NULL);
        taken = (uint32_t)sel_engine_playlist_keys_take(e, &got_id, keys, sizeof keys, &n, &ok);
        if (!taken) usleep(2000);
    }
    CHECK(taken == 1u && got_id == id && ok == 1 && n > 0u);
    CHECK(log_count(logp, ".prg") == 1u);
    CHECK(pump_reads(e, base, 1) >= 1u); /* I1058e: the take posted a folders reload */

    /* 3. Already filled: no new request, no reload. */
    base = reads_now(e);
    CHECK(sel_engine_playlist_keys_post(e, id, sizeof keys) == 1);
    taken = 0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (!taken && ms_since(&t0) < 15000) {
        sel_engine_pump(e, sink, NULL);
        taken = (uint32_t)sel_engine_playlist_keys_take(e, &got_id, keys, sizeof keys, &n, &ok);
        if (!taken) usleep(2000);
    }
    CHECK(taken == 1u && ok == 1 && n > 0u);
    CHECK(log_count(logp, ".prg") == 1u);
    sel_engine_pump(e, sink, NULL);
    CHECK(reads_now(e) == base);

    printf("hippo_lazy_open: playlist %u keys %u prg requests %u\n", (unsigned)id, (unsigned)n, log_count(logp, ".prg"));
    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail != 0) return 1;
    printf("hippo_lazy_open_ok\n");
    return 0;
}
