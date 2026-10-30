/* I790: the source page drill (design doc 2026-10-22-architecture-design.md section 9b) on a 600k-song modland catalogue.
 * Drives sel_library_source_drill (the one entry the source page reads) for every level. Asserts: each level returns at most its
 * window (rows emitted, the sentinel), the first level is the format list (not songs), and every level query stays under 200 ms.
 * Sizes stay empty while songs.size is NULL (I801). */
#include "db.h"
#include "db_internal.h"
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROWS 600000
#define BATCH 50000
#define WINDOW 100
#define MAX_MS 200.0

static int fails;
static uint32_t seen;
static char first[128];
static char first_size[32];

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void sink(void* ctx, const char* const* f) {
    (void)ctx;
    if (seen++ == 0) {
        snprintf(first, sizeof first, "%s", f[0]);
        snprintf(first_size, sizeof first_size, "%s", f[2]);
    }
}

static double now_ms(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

/* Runs one level, asserts window and time; returns the rows emitted. */
static int level(sel_library* lib, const char* name, uint32_t lv, const char* a, const char* b, uint32_t skip) {
    double t0, ms;
    int n;
    char what[128];

    seen = 0;
    first[0] = '\0';
    t0 = now_ms();
    n = sel_library_source_drill(lib, "modland", lv, a, b, skip, WINDOW, sink, NULL);
    ms = now_ms() - t0;
    printf("level %-16s rows=%d  %.1f ms\n", name, n, ms);
    snprintf(what, sizeof what, "%s: rows within the window (%d <= %d)", name, n, WINDOW);
    check(n >= 0 && n <= WINDOW && (uint32_t)n == seen, what);
    snprintf(what, sizeof what, "%s: under %.0f ms (%.1f)", name, MAX_MS, ms);
    check(ms < MAX_MS, what);
    return n;
}

int main(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    char path[512];
    sel_library* lib;
    char sql[900];
    int lo, n;
    char artist[64];

    snprintf(path, sizeof path, "%s/library_source_drill.db", argc > 1 ? argv[1] : ".");
    remove(path);
    lib = sel_library_open(host, sel_machine_fake("a060"), 0, path);
    check(lib != NULL, "open the library");
    if (lib == NULL) return 1;
    sqlite3_exec(lib->db, "DROP TRIGGER songs_ai;", NULL, NULL, NULL);
    for (lo = 1; lo <= ROWS; lo += BATCH) {
        snprintf(sql, sizeof sql,
                 "WITH RECURSIVE n(i) AS (SELECT %d UNION ALL SELECT i + 1 FROM n WHERE i < %d)"
                 " INSERT INTO songs(key, source, path, name, artist, album, format, added)"
                 " SELECT 'modland:' || i, 'modland', '/m/' || i, 'Song ' || i, 'Artist ' || (i * 7919 %% 30011),"
                 " '', CASE i %% 7 WHEN 0 THEN 'Protracker' WHEN 1 THEN 'Fasttracker 2' WHEN 2 THEN 'Impulsetracker'"
                 " ELSE 'format' || (i %% 60) END, 1 FROM n;",
                 lo, lo + BATCH - 1);
        check(sqlite3_exec(lib->db, sql, NULL, NULL, NULL) == SQLITE_OK, "fill");
    }
    sqlite3_exec(lib->db, "INSERT INTO source_new(source, day, n) VALUES('modland', 20000, 12);", NULL, NULL, NULL);

    n = level(lib, "formats", SEL_DRILL_FORMATS, "", "", 0);
    check(n > 0 && n < WINDOW, "first level is the format list, a few dozen rows, not 600k songs");
    check(first_size[0] == '\0', "size stays empty while no song has a size");
    level(lib, "format artists", SEL_DRILL_FORMAT_ARTISTS, "Protracker", "", 0);
    snprintf(artist, sizeof artist, "%s", first);
    level(lib, "format artists 2", SEL_DRILL_FORMAT_ARTISTS, "Protracker", "", WINDOW);
    level(lib, "format songs", SEL_DRILL_FORMAT_SONGS, "Protracker", artist, 0);
    level(lib, "artists A-Z", SEL_DRILL_ARTISTS, "", "", 0);
    level(lib, "artists deep", SEL_DRILL_ARTISTS, "", "", 25000);
    level(lib, "artist songs", SEL_DRILL_ARTIST_SONGS, "Artist 1", "", 0);
    n = level(lib, "new", SEL_DRILL_NEW, "", "", 0);
    check(n == 1 && strcmp(first, "20000") == 0, "New reads source_new");
    sel_library_close(lib);
    remove(path);
    if (fails == 0) puts("library_source_drill_ok");
    return fails != 0;
}
