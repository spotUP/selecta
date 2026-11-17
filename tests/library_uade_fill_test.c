/* I774m R6-R7: the library fills format, channels, publisher and composers of a local song from the uade bake files beside
 * library.db with no network (D2: the bake fills blanks only), a key missing from the bake still reaches the songdb endpoint
 * (a dead local URL counts as the mock: songdb_lookups goes up), and the new query terms return the fixture songs. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include "query.h"
#include "uadedb.h"
#include "xxh32.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void put_file(const char* dir, const char* rel, const char* text) {
    char path[512];
    FILE* f;

    snprintf(path, sizeof path, "%s/%s", dir, rel);
    f = fopen(path, "wb");
    if (f == NULL) exit(2);
    fputs(text, f);
    fclose(f);
}

static void make_dir(const char* dir, const char* rel) {
    char path[512];

    snprintf(path, sizeof path, "%s/%s", dir, rel);
    mkdir(path, 0777);
}

static void scan(sel_library* lib, const char* path) {
    sel_scan_row row;
    sel_scan_batch b;
    sel_message reply;

    memset(&row, 0, sizeof row);
    row.source = "local";
    row.path = path;
    row.name = path;
    memset(&b, 0, sizeof b);
    b.rows = &row;
    b.count = 1;
    b.now = 1;
    check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0, "scan batch");
}

static int count(sel_library* lib, const char* sql) {
    sqlite3_stmt* st = NULL;
    int n = -1;

    if (sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return n;
}

/* Rows the compiled query text returns. */
static int query_rows(sel_library* lib, const char* text) {
    sel_query q;
    sqlite3_stmt* st = NULL;
    unsigned i;
    int n = 0;

    if (!sel_query_compile(text, 1, NULL, NULL, &q)) return -1;
    if (sqlite3_prepare_v2(lib->db, q.sql, -1, &st, NULL) != SQLITE_OK) return -1;
    for (i = 0; i < q.nparams; i++) {
        if (q.params[i].is_int) sqlite3_bind_int64(st, (int)i + 1, q.params[i].i);
        else sqlite3_bind_text(st, (int)i + 1, q.params[i].s, -1, SQLITE_TRANSIENT);
    }
    while (sqlite3_step(st) == SQLITE_ROW) n++;
    sqlite3_finalize(st);
    return n;
}

int main(void) {
    char root[] = "/tmp/selecta_uadefill_XXXXXX";
    char out[300], db[320], known[320], other[320], rows[256], key[SEL_UADE_KEY_CHARS + 1u], skey[340];
    sel_uadedb_stats bst;
    sel_library* lib;
    sel_genre_stats gs;
    sel_genre_op op;
    int64_t id = 0;

    if (mkdtemp(root) == NULL) return 2;
    make_dir(root, "tsv");
    make_dir(root, "tsv/pretty");
    make_dir(root, "tsv/pretty/xxh32");
    make_dir(root, "tsv/pretty/md5");
    make_dir(root, "songdb");
    make_dir(root, "songdb/sources");
    make_dir(root, "songdb/sources/site");
    make_dir(root, "songdb/sources/cd");
    make_dir(root, "out");
    snprintf(out, sizeof out, "%s/out", root);
    snprintf(known, sizeof known, "%s/known.mod", root);
    snprintf(other, sizeof other, "%s/other.mod", root);
    put_file(root, "known.mod", "fixture bytes of a song the bake knows");
    put_file(root, "other.mod", "fixture bytes of a song the bake does not know");
    check(sel_uade_key_file(known, key) == 1, "key of the fixture file");

    snprintf(rows, sizeof rows, "%s\t0\t1000,p\n", key);
    put_file(root, "tsv/pretty/xxh32/songlengths.tsv", rows);
    snprintf(rows, sizeof rows, "%s\tProtracker\t4\n", key);
    put_file(root, "tsv/pretty/xxh32/modinfos.tsv", rows);
    snprintf(rows, sizeof rows, "%s\tGlue Master~Kid\tMahoney\tMusicdisk 1\t1988\n", key);
    put_file(root, "tsv/pretty/xxh32/metadata.tsv", rows);
    put_file(root, "tsv/pretty/md5/modinfos.tsv", "");
    put_file(root, "tsv/pretty/md5/metadata.tsv", "");
    put_file(root, "tsv/pretty/md5/songlengths.tsv", ""); /* I774r: the twin check reads it */
    check(sel_uadedb_bake(root, out, &bst) == 0, "bake");

    snprintf(db, sizeof db, "%s/library.db", out);
    setenv("SELECTA_DEVILBOX_URL", "http://127.0.0.1:1", 1);
    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, db);
    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    scan(lib, known);
    scan(lib, other);

    /* the idle pass hashes the song, stores its key and fills the blanks from the bake: no network involved */
    memset(&op, 0, sizeof op);
    op.op = SEL_DUP_HASH;
    for (;;) {
        id = 0;
        check(sel_genre_call(lib, &op, &id) >= 0, "dup hash");
        if (id == 0) break;
        op.after = id;
    }
    snprintf(skey, sizeof skey, "local:%s", known);
    check(count(lib, "SELECT COUNT(*) FROM songs WHERE uade_key IS NOT NULL") == 2, "both songs have a uade key");
    check(count(lib, "SELECT COUNT(*) FROM songs WHERE format='Protracker' AND channels=4 AND year=1988 AND album='Musicdisk 1' AND publisher='Mahoney'") == 1,
          "known song got format, channels, year, album, publisher");
    check(count(lib, "SELECT COUNT(*) FROM song_composers WHERE tag=0") == 2, "known song got both authors");
    check(count(lib, "SELECT COUNT(*) FROM songs WHERE channels=0 AND format IS NOT 'Protracker'") == 1, "unknown song stays blank");

    /* I774m: a song hashed before the key existed (md5 set, uade_key NULL, blanks unfilled) is caught up by the same pass */
    check(sqlite3_exec(lib->db, "UPDATE songs SET uade_key = NULL, format = NULL, channels = 0, year = NULL, album = NULL, publisher = NULL;"
                                "DELETE FROM song_composers; DELETE FROM song_strings;", NULL, NULL, NULL) == SQLITE_OK, "simulate pre-key rows");
    check(count(lib, "SELECT COUNT(*) FROM songs WHERE COALESCE(md5,'') <> '' AND uade_key IS NULL") == 2, "both songs hashed, no key");
    memset(&op, 0, sizeof op);
    op.op = SEL_DUP_HASH;
    for (;;) {
        id = 0;
        check(sel_genre_call(lib, &op, &id) >= 0, "dup hash catch-up");
        if (id == 0) break;
        op.after = id;
    }
    check(count(lib, "SELECT COUNT(*) FROM songs WHERE uade_key IS NOT NULL") == 2, "hashed songs got their key");
    check(count(lib, "SELECT COUNT(*) FROM songs WHERE format='Protracker' AND channels=4 AND year=1988 AND publisher='Mahoney'") == 1,
          "hashed song got its bake data");

    /* queries */
    check(query_rows(lib, "channels:>3") == 1, "channels term");
    check(query_rows(lib, "channels:>4") == 0, "channels bound");
    check(query_rows(lib, "publisher:mahoney") == 1, "publisher term");
    check(query_rows(lib, "release:musicdisk") == 1, "release term");
    check(query_rows(lib, "found:aminet") == 0 && query_rows(lib, "sametune:x") == 0, "pending terms match nothing");

    /* resolve: a bake key never asks the endpoint, a missing key does */
    memset(&gs, 0, sizeof gs);
    check(sel_library_songdb_resolve(lib, key, known, skey, &gs) == 1, "resolve known");
    check(gs.bake_hits == 1u && gs.songdb_lookups == 0u, "bake hit skips the endpoint");
    snprintf(skey, sizeof skey, "local:%s", other);
    check(sel_uade_key_file(other, key) == 1, "other key");
    check(sel_library_songdb_resolve(lib, key, other, skey, &gs) == 1, "resolve other");
    check(gs.bake_hits == 1u && gs.songdb_lookups == 1u, "missing key reaches the endpoint");

    sel_library_close(lib);
    snprintf(rows, sizeof rows, "rm -rf %s", root);
    if (system(rows) != 0) printf("cleanup failed\n");
    if (fails == 0) printf("library_uade_fill_ok\n");
    return fails == 0 ? 0 : 1;
}
