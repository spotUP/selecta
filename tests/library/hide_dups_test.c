/* I1094c: library.hide_duplicates. The search arms skip songs.dup_of IS NOT NULL in SQL before paging, the switch is
 * sel_library_set_hide_duplicates (like sources_off), the query keys duplicate:yes and copies:<key> show hidden copies, and
 * the radio check sel_library_key_hidden_duplicate follows the switch. Design: thoughts/shared/plans/2026-11-06-I1094-hide-duplicates.md section 3.
 * Fixture: k1 + k2 share an md5 (k2 is the hidden copy, dup_of = k1), k3 is unique, then 300 "bulk" songs of which every 5th is hidden. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void run(sel_library* lib, const char* sql) {
    char* err = NULL;

    if (sqlite3_exec(lib->db, sql, NULL, NULL, &err) != SQLITE_OK) {
        printf("FAIL sql %s: %s\n", sql, err != NULL ? err : "?");
        fails++;
        sqlite3_free(err);
    }
}

static uint32_t search(sel_library* lib, const char* text, char* out, uint32_t cap) {
    sel_lib_job job;

    memset(&job, 0, sizeof job);
    job.text = text;
    job.out = out;
    job.out_cap = cap;
    check(sel_library_run(lib, SEL_LIB_REQ_SEARCH, &job) && job.error[0] == '\0', text);
    return job.count;
}

static int has(const char* out, uint32_t n, const char* key) {
    uint32_t i;

    for (i = 0; i < n; i++, out += strlen(out) + 1u) {
        if (strcmp(out, key) == 0) return 1;
    }
    return 0;
}

int main(void) {
    const sel_host* host = sel_host_default();
    sel_library* lib = sel_library_open(host, sel_machine_fake("a060"), 0, ":memory:");
    static char out[40000];
    char sql[200];
    uint32_t n;
    int i;

    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    check(lib->hide_dups == 1, "default on");
    run(lib, "INSERT INTO songs(key, source, path, name, md5, format) VALUES('local:k1','local','p1','tune one','m1','mod'),"
             "('local:k2','local','p2','tune one','m1','mod'),('local:k3','local','p3','tune three','m3','mod')");
    run(lib, "UPDATE songs SET dup_of = 'local:k1', dup_members = 2 WHERE key = 'local:k2'; UPDATE songs SET dup_members = 2 WHERE key = 'local:k1'");
    for (i = 0; i < 300; i++) {
        snprintf(sql, sizeof sql, "INSERT INTO songs(key, source, path, name, md5, format, dup_of) VALUES('local:b%03d','local','b%03d','bulk %03d','','mod',%s)",
                 i, i, i, i % 5 == 0 ? "'local:k1'" : "NULL");
        run(lib, sql);
    }

    n = search(lib, "tune", out, sizeof out);
    check(n == 2 && has(out, n, "local:k1") && has(out, n, "local:k3") && !has(out, n, "local:k2"), "bare words: hidden copy skipped");
    n = search(lib, "format:mod name:tune", out, sizeof out);
    check(n == 2 && !has(out, n, "local:k2"), "query: hidden copy skipped");
    n = search(lib, "bulk", out, sizeof out);
    check(n == 240, "300 bulk songs, 60 hidden: 240 visible rows");
    n = search(lib, "copies:local:k2", out, sizeof out);
    check(n == 2 && has(out, n, "local:k1") && has(out, n, "local:k2"), "copies: names the whole group, hidden or not");
    n = search(lib, "duplicate:yes name:tune", out, sizeof out);
    check(n == 1 && has(out, n, "local:k2"), "duplicate:yes finds the hidden copy");
    n = search(lib, "format:mod duplicate:no name:tune", out, sizeof out);
    check(n == 2 && !has(out, n, "local:k2"), "duplicate:no");
    check(sel_library_key_hidden_duplicate(lib, "local:k2") == 1 && sel_library_key_hidden_duplicate(lib, "local:k1") == 0, "radio check on");

    sel_library_set_hide_duplicates(lib, "off");
    n = search(lib, "tune", out, sizeof out);
    check(n == 3, "off: bare words show all copies");
    n = search(lib, "bulk", out, sizeof out);
    check(n == 300, "off: 300 bulk rows");
    check(sel_library_key_hidden_duplicate(lib, "local:k2") == 0, "radio check off");
    sel_library_set_hide_duplicates(lib, "on");
    n = search(lib, "tune", out, sizeof out);
    check(n == 2, "on again");

    /* meta fill: the sixth field is the group size above 1, else empty */
    {
        static char keys[400], meta[800];
        sel_lib_job job;
        const char* m = meta;
        int f;

        memset(&job, 0, sizeof job);
        job.text = "tune";
        job.out = keys;
        job.out_cap = sizeof keys;
        job.meta = meta;
        job.meta_cap = sizeof meta;
        check(sel_library_run(lib, SEL_LIB_REQ_SEARCH, &job) && job.count == 2, "meta search");
        for (f = 0; f < 5; f++) m += strlen(m) + 1u; /* k1: title, artist, format, duration, album */
        check(strcmp(m, "2") == 0, "meta: k1 copies 2");
        m += strlen(m) + 1u;
        for (f = 0; f < 5; f++) m += strlen(m) + 1u;
        check(m[0] == '\0', "meta: k3 copies empty");
        check(m + 1u == meta + job.meta_used, "meta: two rows of six fields");
    }

    /* the index arm: an index exported from this catalogue carries dup_of, so a library that only has the index hides k2 too */
    {
        static const char idx_path[] = "hide_dups_index.db";
        sel_library* b;
        uint32_t rows = 0;

        remove(idx_path);
        check(sel_library_export_index(lib, "local", idx_path, &rows) && rows == 303u, "export index");
        b = sel_library_open(host, sel_machine_fake("a060"), 0, ":memory:");
        check(b != NULL && sel_library_attach_index(b, idx_path) && b->idx_dup == 1, "attach index with dup columns");
        if (b != NULL) {
            n = search(b, "tune", out, sizeof out);
            check(n == 2 && has(out, n, "local:k1") && !has(out, n, "local:k2"), "index arm: hidden copy skipped");
            n = search(b, "bulk", out, sizeof out);
            check(n == 240, "index arm: 240 visible bulk rows");
            n = search(b, "duplicate:yes name:tune", out, sizeof out);
            check(n == 1 && has(out, n, "local:k2"), "index arm: duplicate:yes shows the hidden one");
            sel_library_set_hide_duplicates(b, "off");
            n = search(b, "tune", out, sizeof out);
            check(n == 3, "index arm: off shows all");
            sel_library_close(b);
        }
        remove(idx_path);
    }

    sel_library_close(lib);
    if (fails == 0) printf("hide_dups_ok\n");
    return fails != 0;
}
