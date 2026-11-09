/* I1196: the catalogue song count never walks the songs table.
 *
 * Before: sel_library_song_total ran SELECT COUNT(*) FROM songs on the single library worker. COUNT walks every page of the songs key
 * index: on a cold 645 000-song db 1.6-4.5 s, so the sidebar and page reads queued behind it (a click in the first seconds hit nothing)
 * and sel_engine_destroy waited for it at quit. Now the count is the sum of the v8 per-source counts (source_songs), one page.
 *
 * Cost is measured in pages, not milliseconds: a cold cache makes every page a disk read, and the page count is exact and the same on
 * every machine. sqlite3_db_status CACHE_HIT + CACHE_MISS on the library connection counts every page the statement touched.
 * Asserts:
 * 1. the count touches at most MAX_PAGES pages on a SONGS-song library (COUNT(*) touched one page per ~100 keys, hundreds here);
 * 2. the count is exact: it equals COUNT(*) FROM songs;
 * 3. reachability: an engine on that library publishes library_songs == SONGS, sentinel total_reads >= 1 (the rd_total read landed). */
#include "db/db.h"
#include "db/db_internal.h"

#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/machine.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SONGS 20000u
#define MAX_PAGES 8

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static uint32_t reg_count(void) {
    return 0u;
}

static const sel_tag* reg_get(uint32_t i) {
    (void)i;
    return NULL;
}

/* Pages the connection touched so far (cache hits plus misses). Read while the worker idles. */
static int pages(sel_library* lib) {
    int hit = 0, miss = 0, hw = 0;
    sqlite3_db_status(lib->db, SQLITE_DBSTATUS_CACHE_HIT, &hit, &hw, 0);
    sqlite3_db_status(lib->db, SQLITE_DBSTATUS_CACHE_MISS, &miss, &hw, 0);
    return hit + miss;
}

static int64_t count_star(sel_library* lib) {
    sqlite3_stmt* st = NULL;
    int64_t n = -1;
    if (sqlite3_prepare_v2(lib->db, "SELECT COUNT(*) FROM songs", -1, &st, NULL) != SQLITE_OK) return -1;
    if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

int main(void) {
    char dir[] = "/tmp/selecta_total_cost_XXXXXX";
    char db[512], cmd[600];
    sel_engine_config cfg;
    sel_engine_stats stats;
    sel_engine* eng;
    sel_library* lib;
    uint32_t total = 0, i;
    int p0, used;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/library.db", dir);
    lib = sel_library_open(sel_host_default(), sel_machine_probe(sel_host_default()), 0, db);
    CHECK(lib != NULL);
    if (lib == NULL) return 2;
    CHECK(sel_library_gen(lib, SONGS));

    p0 = pages(lib);
    CHECK(sel_library_song_total(lib, &total) == SEL_FOLDER_OK);
    used = pages(lib) - p0;
    printf("song_total=%u pages=%d max=%d\n", (unsigned)total, used, MAX_PAGES);
    CHECK(used <= MAX_PAGES);
    CHECK(total == SONGS);
    CHECK((int64_t)total == count_star(lib));

    memset(&cfg, 0, sizeof cfg);
    cfg.registry_count = reg_count;
    cfg.registry_get = reg_get;
    cfg.library = lib;
    eng = sel_engine_create(sel_host_default(), &cfg);
    CHECK(eng != NULL);
    if (eng == NULL) return 2;
    memset(&stats, 0, sizeof stats);
    for (i = 0; i < 200u && stats.total_reads == 0u; i++) {
        (void)sel_engine_state(eng);
        sel_engine_pump(eng, NULL, NULL);
        sel_engine_pump_reads(eng, NULL, NULL, 50u);
        sel_engine_stats_get(eng, &stats);
    }
    (void)sel_engine_state(eng);
    sel_engine_pump(eng, NULL, NULL);
    printf("total_reads=%u library_songs=%u\n", (unsigned)stats.total_reads, (unsigned)sel_engine_state(eng)->library_songs);
    CHECK(stats.total_reads >= 1u); /* sentinel: the posted catalogue count ran and landed */
    CHECK(sel_engine_state(eng)->library_songs == SONGS);
    sel_engine_destroy(eng);
    sel_library_close(lib);
    snprintf(cmd, sizeof cmd, "rm -rf \"%s\"", dir);
    (void)system(cmd);
    if (g_fail) {
        printf("library_total_cost: %d failures\n", g_fail);
        return 1;
    }
    printf("library_total_cost_ok\n");
    return 0;
}
