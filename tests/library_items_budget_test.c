/* I787: the Your Library category queries on a library of 600 000 songs. Johan's library (594 520 songs) took 10.9 s for Albums and
 * 9.2 s for Artists, because the planner walked every song and probed history per song. This builds a library of that size
 * (40 000 albums, 25 000 artists, 5 000 composers, 8 sources, 3 000 history rows), then times each category read through
 * sel_library_items, the query the sidebar runs. Every category must answer in under SEL_BUDGET_MS warm (the second of two runs),
 * and the Artists and Albums rows must carry the right counts, so a fast wrong query fails too. */
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* 200 ms warm is the target for the shipped (optimised) build. An unoptimised SQLite is about 15 times slower, so a Debug build allows 3 s:
 * the 10 s the old queries took on 594 520 songs still fails it. */
#ifdef NDEBUG
#define SEL_BUDGET_MS 200.0
#else
#define SEL_BUDGET_MS 3000.0
#endif
#define SONGS 600000

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void sql(sqlite3* db, const char* text) {
    char* err = NULL;

    if (sqlite3_exec(db, text, NULL, NULL, &err) != SQLITE_OK) {
        printf("FAIL sql: %s (%s)\n", text, err != NULL ? err : "?");
        fails++;
    }
}

static double now_ms(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

static const struct {
    const char* name;
    uint32_t kind;
} cat[] = {{"playlists", SEL_LIB_ITEM_PLAYLIST}, {"albums", SEL_LIB_ITEM_ALBUM},     {"artists", SEL_LIB_ITEM_ARTIST},
           {"composers", SEL_LIB_ITEM_COMPOSER}, {"sources", SEL_LIB_ITEM_SOURCE}};

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_itemsbudget_XXXXXX";
    char path[256], gen[1024];
    static sel_library_item rows[64];
    sel_library* lib;
    uint32_t k, n, total, i;
    double worst = 0.0;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(path, sizeof path, "%s/lib.db", dir);
    lib = sel_library_open(host, sel_machine_probe(host), 0, path);
    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    sql(lib->db, "PRAGMA synchronous=OFF");
    /* The albums insert trigger looks each album up by name and albums has no index on it, so a bulk load of this size is quadratic (found by
     * I787, not part of it). The test builds the library with a scratch index; the queries under test never read it. */
    sql(lib->db, "CREATE INDEX albums_load ON albums(name)");
    /* The library keeps SQLite on a small heap (Amiga), so the songs go in 10 000-row batches, one transaction each. */
    for (i = 0; i < SONGS; i += 10000u) {
        snprintf(gen, sizeof gen,
                 "WITH RECURSIVE g(i) AS (SELECT %u UNION ALL SELECT i+1 FROM g WHERE i<%u) "
                 "INSERT INTO songs(key, source, path, name, artist, album, added) SELECT 's:'||i, 'src'||(i%%8), 'p'||i, 'song '||i, "
                 "'Artist '||((i%%40000)%%25000), 'Album '||(i%%40000), 1000+i FROM g",
                 i, i + 9999u);
        sql(lib->db, gen);
    }
    sql(lib->db, "INSERT INTO song_composers(song_key, canon, display) SELECT key, 'c'||(id%5000), 'Composer '||(id%5000) FROM songs WHERE id%6=0");
    sql(lib->db, "INSERT INTO history(key, played_at) SELECT key, 5000000+id FROM songs WHERE id%200=0");
    sql(lib->db, "DROP INDEX albums_load");
    sql(lib->db, "ANALYZE");
    sql(lib->db, "PRAGMA optimize");

    for (k = 0; k < sizeof cat / sizeof cat[0]; k++) {
        double t[2];

        for (i = 0; i < 2; i++) {
            double t0 = now_ms();

            total = 0;
            n = sel_library_items(lib, SEL_LIB_ITEM_BIT(cat[k].kind), 0, rows, 64u, &total);
            t[i] = now_ms() - t0;
        }
        printf("items_budget %s rows=%u cold=%.0f ms warm=%.0f ms\n", cat[k].name, (unsigned)total, t[0], t[1]);
        if (t[1] > worst) worst = t[1];
        check(t[1] < SEL_BUDGET_MS, cat[k].name);
        check(n > 0 || cat[k].kind == SEL_LIB_ITEM_PLAYLIST, "rows returned");
        if (cat[k].kind == SEL_LIB_ITEM_ARTIST) {
            check(total == 25000u, "25000 artist rows");
        }
        if (cat[k].kind == SEL_LIB_ITEM_ALBUM) check(total == 40000u, "40000 album rows");
    }
    printf("items_budget worst warm %.0f ms (budget %.0f)\n", worst, SEL_BUDGET_MS);
    /* I1400a: Home's shelves (Recently Played, Your Albums, Your Composers) count each album's and composer's plays. With history
     * driving that per-row count, Home held the library worker 11 s at start on Johan's library and the first search waited behind it. */
    {
        static char home[65536];
        uint32_t lines = 0;
        double t[2];

        for (i = 0; i < 2; i++) {
            double t0 = now_ms();

            check(sel_library_home_chip(lib, SEL_HOME_CHIP_ALL, 10, home, sizeof home, &lines) != 0, "home read");
            t[i] = now_ms() - t0;
        }
        printf("items_budget home lines=%u cold=%.0f ms warm=%.0f ms (budget %.0f)\n", (unsigned)lines, t[0], t[1], 10.0 * SEL_BUDGET_MS);
        check(t[1] < 10.0 * SEL_BUDGET_MS, "home under budget");
        check(strstr(home, "Recently Played") != NULL && strstr(home, "Your Albums") != NULL && strstr(home, "Your Composers") != NULL,
              "home shelves listed");
    }
    {
        /* I1400c: the Home date lists (New This Week, From your library, More Like) filter and order songs on `added`; without songs_added each
         * scanned every song on the one library worker, which delayed a search typed during the first seconds. The plan must use the index. */
        static const char* const q[] = {
            "SELECT COALESCE(name, key) FROM songs WHERE added >= CAST(strftime('%s','now') AS INTEGER) - 604800 ORDER BY added DESC, key LIMIT 10",
            "SELECT key FROM songs WHERE key NOT IN (SELECT key FROM history) AND added < CAST(strftime('%s','now') AS INTEGER) - 604800"
            " ORDER BY added DESC, key LIMIT 10"};
        uint32_t qi;

        for (qi = 0; qi < sizeof q / sizeof q[0]; qi++) {
            char plan[1024], text[1200];
            sqlite3_stmt* st = NULL;
            double t0 = now_ms();

            plan[0] = '\0';
            snprintf(text, sizeof text, "EXPLAIN QUERY PLAN %s", q[qi]);
            check(sqlite3_prepare_v2(lib->db, text, -1, &st, NULL) == SQLITE_OK, "plan prepare");
            while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {
                strncat(plan, (const char*)sqlite3_column_text(st, 3), sizeof plan - strlen(plan) - 2u);
                strcat(plan, "\n");
            }
            sqlite3_finalize(st);
            check(strstr(plan, "songs_added") != NULL, "date list uses songs_added");
            if (strstr(plan, "songs_added") == NULL) printf("plan %u:\n%s", (unsigned)qi, plan);
            st = NULL;
            check(sqlite3_prepare_v2(lib->db, q[qi], -1, &st, NULL) == SQLITE_OK, "date list prepare");
            while (st != NULL && sqlite3_step(st) == SQLITE_ROW) {}
            sqlite3_finalize(st);
            printf("items_budget date list %u: %.0f ms\n", (unsigned)qi, now_ms() - t0);
            check(now_ms() - t0 < SEL_BUDGET_MS, "date list under budget");
        }
    }
    sel_library_close(lib);
    {
        char cmd[300];

        snprintf(cmd, sizeof cmd, getenv("SELECTA_ITEMS_BUDGET_KEEP") ? "echo kept %s" : "rm -rf %s", dir);
        if (system(cmd) != 0) fails++;
    }
    if (fails == 0) printf("library_items_budget_ok\n");
    return fails != 0;
}
