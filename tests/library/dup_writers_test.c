/* I1094b: every md5 writer keeps songs.dup_of / dup_members equal to a fresh recompute (design
 * thoughts/shared/plans/2026-11-06-I1094-hide-duplicates.md section 3 "Maintenance"). After each writer the maintained columns are
 * snapshotted, sel_dup_rebuild recomputes everything, and the two snapshots must be identical. The regroup / rebuild counters are the
 * sentinels that the writer ran the maintenance (so removing one call fails the counter check and usually the snapshot too).
 *   single-row writers: store_hash (SEL_DUP_HASH), op_store (SEL_DUP_STORE), annotate (HVSC md5), remove, song delete, move
 *   bulk writers: scan batch, TSV import, sel_library_gen
 * `dup_writers_test bench` prints the catalogue-sync import time on the I847a perf catalogue instead. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

/* key|dup_of|dup_members of every song, in key order. */
static char* snapshot(sqlite3* db) {
    sqlite3_stmt* st = NULL;
    size_t cap = 1 << 16, len = 0;
    char* out = malloc(cap);

    out[0] = '\0';
    if (sqlite3_prepare_v2(db, "SELECT key, COALESCE(dup_of, '-'), dup_members FROM songs ORDER BY key", -1, &st, NULL) != SQLITE_OK) return out;
    while (sqlite3_step(st) == SQLITE_ROW) {
        char line[1600];
        int n = snprintf(line, sizeof line, "%s|%s|%d\n", sqlite3_column_text(st, 0), sqlite3_column_text(st, 1), sqlite3_column_int(st, 2));

        if (len + (size_t)n + 1 > cap) out = realloc(out, cap *= 2);
        memcpy(out + len, line, (size_t)n + 1);
        len += (size_t)n;
    }
    sqlite3_finalize(st);
    return out;
}

/* The maintained columns equal a fresh recompute: snapshot, rebuild, snapshot, compare. */
static void invariant(sel_library* lib, const char* what) {
    char* a = snapshot(lib->db);
    char* b;
    char msg[160];

    { int rrc = sel_dup_rebuild(lib->db); if (rrc != SQLITE_OK) fprintf(stderr, "rebuild rc=%d %s\n", rrc, sqlite3_errmsg(lib->db)); check(rrc == SQLITE_OK, "rebuild"); }
    b = snapshot(lib->db);
    snprintf(msg, sizeof msg, "%s: dup_of / dup_members differ from a fresh recompute", what);
    if (strcmp(a, b) != 0) printf("--- maintained\n%s--- recomputed\n%s", a, b);
    check(strcmp(a, b) == 0, msg);
    free(a);
    free(b);
}

static void row(sel_scan_row* r, const char* source, const char* path, const char* md5, int64_t size) {
    memset(r, 0, sizeof *r);
    r->source = source;
    r->path = path;
    r->name = path;
    r->artist = "A";
    r->album = "";
    r->md5 = md5;
    r->format = "ProTracker";
    r->size = size;
}

static int scan(sel_library* lib, const sel_scan_row* rows, uint32_t n) {
    sel_scan_batch b;
    sel_message reply;

    memset(&b, 0, sizeof b);
    b.rows = rows;
    b.count = n;
    b.now = 1000;
    memset(&reply, 0, sizeof reply);
    return sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0;
}

static double now_ms(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1e6;
}

/* Catalogue sync import on the perf catalogue: `songs` generated rows, then `import` rows upserted in batches of 256, a quarter of
 * them carrying a md5 an existing song already has (the writer that keeps groups pays for those). */
static int bench(uint32_t songs, uint32_t import, const char* path) {
    sel_library* lib;
    sel_scan_row* rows = calloc(256, sizeof *rows);
    char (*paths)[40] = calloc(256, sizeof *paths);
    char (*sums)[24] = calloc(256, sizeof *sums);
    double t0, t1;
    uint32_t i, n = 0;
    int ok = 1;

    /* A file DB, as on a real machine: a :memory: DB keeps every page in the tier's heap and runs out of it at ~7500 songs by itself. */
    if (strcmp(path, ":memory:") != 0) remove(path);
    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, path);
    if (lib == NULL || !sel_library_gen(lib, songs)) return 1;
    t0 = now_ms();
    for (i = 0; i < import && ok; i++) {
        snprintf(paths[n], sizeof paths[n], "sync/new%u.mod", i);
        if (i % 4 == 0) snprintf(sums[n], sizeof sums[n], "md5-%u", (i * 37u) % (songs / 40u));
        else sums[n][0] = '\0';
        row(&rows[n], "sync", paths[n], sums[n], 1000 + i % 777);
        if (++n == 256 || i + 1 == import) {
            ok = scan(lib, rows, n);
            n = 0;
        }
    }
    t1 = now_ms();
    invariant(lib, "catalogue sync import (256-row scan batches)"); /* maintained columns == fresh sel_dup_rebuild */
    if (fails) ok = 0;
    printf("sync_import songs=%u rows=%u ok=%d ms=%.1f\n", songs, import, ok, t1 - t0);
    sel_library_close(lib);
    free(rows);
    free(paths);
    free(sums);
    return ok ? 0 : 1;
}

/* `dup_writers_test probe N [db] [txn]`: heap high water of one sel_dup_rebuild on the a060 tier after sel_library_gen(N), on a fresh
 * library and again with every group already set (txn: inside one transaction, as the bulk writers and the v18 migration run it). */
static int probe(uint32_t songs, const char* path, int txn) {
    sel_library* lib;
    sqlite3_int64 cur, hw, worst = 0;
    double t0;
    int gen_ok, rc = SQLITE_OK, i;

    if (strcmp(path, ":memory:") != 0) remove(path);
    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, path);
    if (lib == NULL) return 1;
    gen_ok = sel_library_gen(lib, songs);
    for (i = 0; i < 2 && rc == SQLITE_OK; i++) {
        if (txn) sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL);
        sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &cur, &hw, 1);
        t0 = now_ms();
        rc = sel_dup_rebuild(lib->db);
        sqlite3_status64(SQLITE_STATUS_MEMORY_USED, &cur, &hw, 1);
        if (txn) sqlite3_exec(lib->db, rc == SQLITE_OK ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);
        printf("probe gen=%d songs=%u rebuild rc=%d hw=%lld ms=%.1f\n", gen_ok, songs, rc, hw, now_ms() - t0);
        if (hw > worst) worst = hw;
    }
    sel_library_close(lib);
    /* I1094b bound: the one-statement rebuild peaked at 4.26 MB here (20000 songs, in a transaction); the sliced one at 0.67 MB. */
    if (gen_ok && rc == SQLITE_OK && worst < 1024 * 1024) printf("dup_rebuild_heap_ok\n");
    return gen_ok && rc == SQLITE_OK ? 0 : 1;
}

int main(int argc, char** argv) {
    sel_library* lib;
    sel_scan_row rows[8];
    sel_annotation an[1];
    sel_annotate_batch ab;
    sel_remove_batch rb;
    sel_move mv;
    sel_message reply;
    sel_genre_op op;
    sel_lib_job job;
    sel_scan_row mvto;
    const char* gone[1];
    char hashfile[64];
    unsigned long rg, rd;
    FILE* f;

    if (argc > 2 && strcmp(argv[1], "probe") == 0) return probe((uint32_t)atoi(argv[2]), argc > 3 ? argv[3] : ":memory:", argc > 4);
    if (argc > 1 && strcmp(argv[1], "bench") == 0) return bench(argc > 2 ? (uint32_t)atoi(argv[2]) : 100000u, argc > 3 ? (uint32_t)atoi(argv[3]) : 20000u, argc > 4 ? argv[4] : ":memory:");
    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, ":memory:");
    check(lib != NULL, "open");
    if (lib == NULL) return 1;

    /* scan batch: groups x (local + modland + hvsc) and y (two modland), loner z */
    rg = sel_db_dup_regroups();
    rd = sel_db_dup_rebuilds();
    row(&rows[0], "local", "a.mod", "x", 50);
    row(&rows[1], "modland", "b.mod", "x", 90);
    row(&rows[2], "hvsc", "c.sid", "x", 99);
    row(&rows[3], "modland", "d.mod", "y", 10);
    row(&rows[4], "modland", "e.mod", "y", 20);
    row(&rows[5], "local", "f.mod", "z", 5);
    row(&rows[6], "local", "g.mod", "", 5);
    check(scan(lib, rows, 7), "scan");
    check(sel_db_dup_regroups() > rg || sel_db_dup_rebuilds() > rd, "scan batch ran the group maintenance");
    invariant(lib, "scan batch");

    /* rescan of an existing path with a changed md5: leaves group y, joins group x */
    row(&rows[0], "modland", "e.mod", "x", 20);
    check(scan(lib, rows, 1), "rescan");
    invariant(lib, "rescan changes md5");

    /* annotate (HVSC Songlengths md5 onto an existing song): moves hvsc c.sid out of x into z */
    an[0].path = "c.sid";
    an[0].md5 = "z";
    an[0].duration_ms = 1000;
    an[0].stickers = NULL;
    an[0].sticker_n = 0;
    ab.source = "hvsc";
    ab.items = an;
    ab.count = 1;
    rg = sel_db_dup_regroups();
    memset(&reply, 0, sizeof reply);
    check(sel_library_request(lib, SEL_LIB_REQ_ANNOTATE, &ab, &reply) && reply.args[1].v.i == 0, "annotate");
    check(sel_db_dup_regroups() > rg, "annotate regrouped");
    invariant(lib, "annotate");

    /* SEL_DUP_STORE (op_store): the local song f.mod gets the md5 of group x */
    memset(&op, 0, sizeof op);
    op.op = SEL_DUP_STORE;
    op.key = "local:f.mod";
    op.value = "x";
    rg = sel_db_dup_regroups();
    memset(&reply, 0, sizeof reply);
    check(sel_library_request(lib, SEL_LIB_REQ_GENRE, &op, &reply), "dup store");
    check(sel_db_dup_regroups() > rg, "op_store regrouped");
    invariant(lib, "op_store");

    /* SEL_DUP_HASH (store_hash): g.mod (no md5) is a real file with the same bytes as nothing else, hashed and grouped */
    snprintf(hashfile, sizeof hashfile, "dup_writers_%d.bin", (int)clock());
    f = fopen(hashfile, "wb");
    check(f != NULL, "hash file");
    if (f != NULL) {
        fputs("selecta", f);
        fclose(f);
    }
    row(&rows[0], "local", hashfile, "", 7);
    row(&rows[1], "local", "g2.mod", "", 7);
    check(scan(lib, rows, 2), "scan hash rows");
    rg = sel_db_dup_regroups();
    check(sel_library_hash_local(lib) >= 1, "dup hash"); /* the unreadable g.mod stays unhashed; the real file is hashed (store_hash) */
    remove(hashfile);
    check(sel_db_dup_regroups() > rg, "store_hash regrouped");
    invariant(lib, "store_hash");

    /* move: the best copy of group x changes its key */
    mvto = rows[0];
    row(&mvto, "local", "a_moved.mod", "x", 50);
    mv.source = "local";
    mv.from = "a.mod";
    mv.to = &mvto;
    memset(&reply, 0, sizeof reply);
    check(sel_library_request(lib, SEL_LIB_REQ_MOVE, &mv, &reply) && reply.args[1].v.i == 0, "move");
    invariant(lib, "move");

    /* remove (changelog delete) of the best copy of x */
    gone[0] = "a_moved.mod";
    rb.source = "local";
    rb.paths = gone;
    rb.count = 1;
    rg = sel_db_dup_regroups();
    memset(&reply, 0, sizeof reply);
    check(sel_library_request(lib, SEL_LIB_REQ_REMOVE, &rb, &reply) && reply.args[1].v.i == 0, "remove");
    check(sel_db_dup_regroups() > rg, "remove regrouped");
    invariant(lib, "remove");

    /* song delete (Remove from library) of the best copy of group y */
    rg = sel_db_dup_regroups();
    {
        uint32_t removed = 0;

        check(sel_library_song_remove(lib, "modland:d.mod", &removed) == SEL_FOLDER_OK && removed == 1, "song remove");
    }
    check(sel_db_dup_regroups() > rg, "song delete regrouped");
    invariant(lib, "song delete");

    /* TSV import changes the source of an existing key (the rank input) and adds a duplicate-free row: one rebuild */
    rd = sel_db_dup_rebuilds();
    memset(&job, 0, sizeof job);
    job.text = "modland:b.mod\tlocal\tb.mod\tb\tA\tProTracker\t0\nnew:1\tnew\tp\tn\tA\tSID\t0\n";
    check(sel_library_run(lib, SEL_LIB_REQ_IMPORT, &job) && job.count == 2, "import");
    check(sel_db_dup_rebuilds() > rd, "TSV import rebuilt");
    invariant(lib, "TSV import");

    printf("dup_regroups=%lu dup_rebuilds=%lu\n", sel_db_dup_regroups(), sel_db_dup_rebuilds());
    sel_library_close(lib);
    if (fails == 0) printf("dup_writers_ok\n");
    return fails != 0;
}
