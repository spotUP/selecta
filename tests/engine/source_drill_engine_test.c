/* I790 (design doc 2026-10-22-architecture-design.md section 9b "Source page drill"): the source page through the engine's real command path.
 * COLLECTION_OPEN of the modland-like source (60 000 songs) must build the drill's first level (Formats, Platforms, Artists, New, not songs),
 * and every deeper level (key with a drill path) stays under SEL_DRILL_WINDOW rows. sel_engine_stats.drill_rows is the sentinel that the
 * library drill emitted the rows; with coll_build_page back on the whole-source SONGS query the page holds 60 000 songs and this fails. */
#include <selecta/engine.h>
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include <selecta/registry.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"

#define ROWS 60000

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void quiet(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static const sel_collection_page* open_page(sel_engine* e, const char* key) {
    static char pool[512];
    char line[400];

    snprintf(line, sizeof line, "COLLECTION_OPEN %u \"%s\"", (unsigned)SEL_LIB_ITEM_SOURCE, key);
    CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
    (void)sel_engine_pump(e, quiet, NULL);
    { /* I910: the page is built by a posted read; pump until it lands (bounded, 5 s) */
        int i;
        for (i = 0; i < 5000 && sel_engine_reads_pending(e); i++) {
            usleep(1000);
            (void)sel_engine_pump(e, quiet, NULL);
        }
    }
    return sel_engine_state(e)->collection;
}

/* The first column of row i of a group page ("name\0count\0size\0" triples). */
static const char* group_name(const sel_collection_page* pg, uint32_t i) {
    const char* r = pg->rows;
    while (i-- > 0u) r += strlen(r) + 1u, r += strlen(r) + 1u, r += strlen(r) + 1u;
    return r;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_srcdrill_XXXXXX";
    char db[256], sql[900], key[200];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_engine_stats st;
    const sel_collection_page* pg;
    uint32_t before;

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
    sqlite3_exec(lib->db, "DROP TRIGGER songs_ai;", NULL, NULL, NULL);
    snprintf(sql, sizeof sql,
             "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < %d)"
             " INSERT INTO songs(key, source, path, name, artist, album, format, added)"
             " SELECT 'modland:' || i, 'modland', '/m/' || i || '.mod', 'Song ' || i, 'Artist ' || (i %% 5000), '',"
             " CASE i %% 3 WHEN 0 THEN 'Protracker' WHEN 1 THEN 'Fasttracker 2' ELSE 'Impulsetracker' END, 1 FROM n;",
             ROWS);
    CHECK(sqlite3_exec(lib->db, sql, NULL, NULL, NULL) == SQLITE_OK);
    CHECK(sqlite3_exec(lib->db, "INSERT INTO source_new(source, day, n) VALUES('modland', 20000, 12);", NULL, NULL, NULL) == SQLITE_OK);

    /* First level: the drill, not the songs. */
    pg = open_page(e, "modland");
    CHECK(pg != NULL && pg->kind == SEL_LIB_ITEM_SOURCE);
    CHECK(pg->drill == SEL_DRILL_PAGE_ROOT);
    CHECK(pg->songs == 4u);
    CHECK(pg != NULL && strcmp(group_name(pg, 0), "formats") == 0 && strcmp(group_name(pg, 1), "platforms") == 0 &&
          strcmp(group_name(pg, 2), "artists") == 0 && strcmp(group_name(pg, 3), "new") == 0);
    sel_engine_stats_get(e, &st);
    CHECK(st.drill_rows > 0u); /* sentinel: the library drill ran for this page */
    CHECK(pg->songs < ROWS && pg->drill_rows <= 3u * SEL_DRILL_WINDOW);

    /* Deeper levels stay inside the window; each reads through the drill (sentinel moves). */
    snprintf(key, sizeof key, "modland\x1f" "format\x1f" "Protracker");
    before = st.drill_rows;
    pg = open_page(e, key);
    CHECK(pg->drill == SEL_DRILL_PAGE_GROUPS && pg->songs == SEL_DRILL_WINDOW); /* 1667 artists in the format, 200 in the window */
    sel_engine_stats_get(e, &st);
    CHECK(st.drill_rows == before + SEL_DRILL_WINDOW);

    snprintf(key, sizeof key, "modland\x1f" "artists");
    pg = open_page(e, key);
    CHECK(pg->drill == SEL_DRILL_PAGE_GROUPS && pg->songs == SEL_DRILL_WINDOW);
    snprintf(key, sizeof key, "modland\x1f" "artists\x1e" "200");
    pg = open_page(e, key);
    CHECK(pg->songs == SEL_DRILL_WINDOW); /* the next window */

    snprintf(key, sizeof key, "modland\x1f" "artist\x1f" "Artist 7");
    pg = open_page(e, key);
    CHECK(pg->drill == SEL_DRILL_PAGE_SONGS && pg->songs == ROWS / 5000);

    snprintf(key, sizeof key, "modland\x1f" "platforms");
    pg = open_page(e, key);
    CHECK(pg->drill == SEL_DRILL_PAGE_GROUPS && pg->songs >= 1u && pg->songs <= SEL_DRILL_WINDOW);

    snprintf(key, sizeof key, "modland\x1f" "new");
    pg = open_page(e, key);
    CHECK(pg->drill == SEL_DRILL_PAGE_GROUPS && pg->songs == 1u && strcmp(group_name(pg, 0), "20000") == 0);

    /* Recents keeps the source id, not the drill path. */
    {
        sqlite3_stmt* q = NULL;
        int rows = -1, ok = 0;
        if (sqlite3_prepare_v2(lib->db, "SELECT count(*), sum(id = 'modland') FROM opened WHERE kind = 7;", -1, &q, NULL) == SQLITE_OK &&
            sqlite3_step(q) == SQLITE_ROW) {
            rows = sqlite3_column_int(q, 0);
            ok = sqlite3_column_int(q, 1);
        }
        sqlite3_finalize(q);
        CHECK(rows == 1 && ok == 1);
    }

    /* I935: an open taken while the collection read is in flight waits for the next post; quit right after must still write its Recents
     * touch. The source root read over 60 000 songs is in flight when the album open is taken in the same pump. */
    {
        static char pool_a[512], pool_b[512];
        char line[400];
        snprintf(line, sizeof line, "COLLECTION_OPEN %u \"modland\"", (unsigned)SEL_LIB_ITEM_SOURCE);
        CHECK(sel_engine_send_line(e, line, pool_a, sizeof pool_a) == 0);
        snprintf(line, sizeof line, "COLLECTION_OPEN %u \"Quit Album\"", (unsigned)SEL_LIB_ITEM_ALBUM);
        CHECK(sel_engine_send_line(e, line, pool_b, sizeof pool_b) == 0);
        (void)sel_engine_pump(e, quiet, NULL);
    }
    sel_engine_destroy(e);
    {
        sqlite3_stmt* q = NULL;
        int rows = -1;
        if (sqlite3_prepare_v2(lib->db, "SELECT count(*) FROM opened WHERE kind = 4 AND id = 'Quit Album';", -1, &q, NULL) == SQLITE_OK &&
            sqlite3_step(q) == SQLITE_ROW) {
            rows = sqlite3_column_int(q, 0);
        }
        sqlite3_finalize(q);
        CHECK(rows == 1);
    }
    sel_library_close(lib);
    snprintf(sql, sizeof sql, "rm -rf %s", dir);
    if (system(sql) != 0) g_fail++;
    if (g_fail == 0) puts("source_drill_engine_ok");
    return g_fail != 0;
}
