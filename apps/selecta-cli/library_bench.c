/* I847a: `selecta-cli library gen --songs <n> --out <path>` and `library bench --db <path> [--runs <n>]`
 * (design 2026-10-31-interactive-path.md section 4). gen writes the deterministic catalogue of sel_library_gen; bench times the engine's
 * interactive library reads of section 1.4 (search first page, chip, list switch, source page, home, play lookup) and prints one line per
 * measure: `<key>_p95=<ms> <key>_calls=<n> rows=<n>`. The calls sentinel counts the queries that ran; a measure that read nothing prints
 * rows=0 and the run fails. */
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include "cli.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

static int cmp_d(const void* a, const void* b) {
    double x = *(const double*)a;
    double y = *(const double*)b;

    return x < y ? -1 : x > y;
}

static const char* opt(int argc, char** argv, const char* name) {
    int i;

    for (i = 0; i + 1 < argc; i++) {
        if (strcmp(argv[i], name) == 0) return argv[i + 1];
    }
    return NULL;
}

int sel_cli_library_gen(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* out = opt(argc, argv, "--out");
    const char* n = opt(argc, argv, "--songs");
    uint32_t songs = n != NULL ? (uint32_t)strtoul(n, NULL, 10) : 600000u;
    sel_library* lib;
    uint32_t total = 0;
    double t0 = now_ms();

    if (out == NULL) {
        fputs("usage: selecta-cli library gen [--songs <n>] --out <path>\n", stderr);
        return 2;
    }
    remove(out);
    lib = sel_library_open(host, sel_machine_probe(host), 0, out);
    if (lib == NULL || !sel_library_gen(lib, songs)) {
        fputs("library gen: failed\n", stderr);
        if (lib != NULL) sel_library_close(lib);
        return 1;
    }
    sel_library_song_total(lib, &total);
    sel_library_close(lib);
    printf("gen_songs=%u gen_ms=%.0f\n", (unsigned)total, now_ms() - t0);
    return 0;
}

typedef struct {
    sel_library* lib;
    uint32_t rows;
    uint32_t calls;
} bench_ctx;

static void count_row(void* ctx, const char* const* f) {
    (void)f;
    ((bench_ctx*)ctx)->rows++;
}

static char keys[64 * 1024];
static char meta[64 * 1024];

/* One timed read of kind `what` (index into the table in sel_cli_library_bench); returns rows read. */
static uint32_t run_one(bench_ctx* b, int kind, const char* arg) {
    sel_lib_job job;
    static sel_library_item items[64];
    char out[16384];
    uint32_t total = 0, lines = 0;

    b->calls++;
    switch (kind) {
    case 0: /* search: first page, 200 rows, with display meta */
        memset(&job, 0, sizeof job);
        job.text = arg;
        job.now = (long long)time(NULL);
        job.out = keys;
        job.out_cap = sizeof keys;
        job.meta = meta;
        job.meta_cap = sizeof meta;
        job.scan_max = 200;
        if (!sel_library_run(b->lib, SEL_LIB_REQ_SEARCH, &job) || job.error[0] != '\0') return 0;
        return job.count;
    case 1: /* list switch */
        {
            int n = sel_library_items(b->lib, SEL_LIB_ITEM_BIT(arg[0] == 'a' ? SEL_LIB_ITEM_ARTIST : SEL_LIB_ITEM_ALBUM), 0, items, 64u, &total);
            return n > 0 ? (uint32_t)n : 0;
        }
    case 2: /* source page: first level (formats of Modland) */
        b->rows = 0;
        sel_library_source_drill(b->lib, "modland", 0, "", "", 0, 200, count_row, b);
        return b->rows;
    case 3: /* home first paint */
        return sel_library_home(b->lib, 12, out, sizeof out, &lines) ? lines + 1 : 0;
    default: /* play lookup: best source of a Modland song with a twin */
        return sel_library_best_source(b->lib, arg, out, sizeof out) ? 1 : 0;
    }
}

int sel_cli_library_bench(int argc, char** argv) {
    static const struct {
        const char* key;
        int kind;
        const char* arg;
    } m[] = {{"search_first_page_a", 0, "a"},
             {"search_first_page_ch", 0, "ch"},
             {"search_first_page_chip", 0, "chip"},
             {"search_first_page_purple_motion", 0, "purple motion"},
             {"search_first_page_4mat", 0, "4mat"},
             {"chip_switch", 0, "format:Protracker"},
             {"list_switch_artists", 1, "artists"},
             {"list_switch_albums", 1, "albums"},
             {"source_page", 2, ""},
             {"home_first_paint", 3, ""},
             {"play_lookup", 4, "modland:2001"}};
    const sel_host* host = sel_host_default();
    const char* db = opt(argc, argv, "--db");
    const char* r = opt(argc, argv, "--runs");
    int runs = r != NULL ? atoi(r) : 50;
    bench_ctx b;
    double t[200];
    unsigned k;
    int i, bad = 0;

    if (db == NULL || runs < 1 || runs > 200) {
        fputs("usage: selecta-cli library bench --db <path> [--runs <1..200>]\n", stderr);
        return 2;
    }
    b.lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    if (b.lib == NULL) {
        fputs("library bench: cannot open the db\n", stderr);
        return 1;
    }
    for (k = 0; k < sizeof m / sizeof m[0]; k++) {
        uint32_t rows = 0;

        b.calls = 0;
        run_one(&b, m[k].kind, m[k].arg); /* warm-up, not timed (the budgets are warm OS cache) */
        b.calls = 0;
        for (i = 0; i < runs; i++) {
            double t0 = now_ms();

            rows = run_one(&b, m[k].kind, m[k].arg);
            t[i] = now_ms() - t0;
        }
        qsort(t, (size_t)runs, sizeof t[0], cmp_d);
        printf("%s_p95=%.1f %s_calls=%u rows=%u\n", m[k].key, t[(runs * 95 + 99) / 100 - 1], m[k].key, (unsigned)b.calls, (unsigned)rows);
        fflush(stdout); /* a slow measure must not hide the lines before it */
        if (rows == 0) bad++;
    }
    sel_library_close(b.lib);
    return bad != 0;
}
