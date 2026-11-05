/* I1077b: the library's write generation counts committed write transactions (design 2026-11-05-I1077-live-view-refresh.md 3.1).
 * Driven through the store's own entry points, so the sqlite commit hook is what moves the count:
 *   one autocommit write (like)            -> +1
 *   a read (liked)                         -> +0
 *   a 100-row scan batch (one BEGIN/COMMIT) -> +1
 *   a ROLLBACK                              -> +0
 *   a pin batch                             -> write_gen +1 and pin_gen +1; a non-pin write leaves pin_gen alone
 * Sentinel: the hook must have run (write_gen > 0 at the end) and "hook_commits=N" prints the total. */
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

static void delta(const char* what, uint32_t before, uint32_t after, uint32_t want) {
    char buf[96];

    snprintf(buf, sizeof buf, "%s: write_gen %u -> %u (want +%u)", what, before, after, want);
    check(after - before == want, buf);
}

int main(void) {
    const sel_host* host = sel_host_default();
    sel_library* lib = sel_library_open(host, sel_machine_fake("a060"), 0, ":memory:");
    sel_scan_row rows[100];
    char paths[100][24];
    sel_scan_batch b;
    sel_message reply;
    uint32_t g0, g1, p0;
    int i;

    check(lib != NULL, "open");
    if (lib == NULL) return 1;

    g0 = sel_library_write_gen(lib);
    check(sel_library_like(lib, "testsrc:a", 1000), "like");
    g1 = sel_library_write_gen(lib);
    delta("one autocommit write", g0, g1, 1u);

    g0 = g1;
    (void)sel_library_liked(lib, "testsrc:a");
    g1 = sel_library_write_gen(lib);
    delta("a read", g0, g1, 0u);

    memset(rows, 0, sizeof rows);
    for (i = 0; i < 100; i++) {
        snprintf(paths[i], sizeof paths[i], "dir/song%03d.mod", i);
        rows[i].source = "testsrc";
        rows[i].path = paths[i];
        rows[i].name = paths[i];
        rows[i].artist = "A";
        rows[i].album = "";
        rows[i].md5 = "";
        rows[i].format = "ProTracker";
    }
    memset(&b, 0, sizeof b);
    b.rows = rows;
    b.count = 100;
    b.now = 1000;
    g0 = sel_library_write_gen(lib);
    p0 = sel_library_pin_gen(lib);
    memset(&reply, 0, sizeof reply);
    check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0, "scan batch");
    g1 = sel_library_write_gen(lib);
    delta("a 100-row scan batch", g0, g1, 1u);
    check(sel_library_pin_gen(lib) == p0, "a scan batch is not a pin commit");

    g0 = g1;
    check(sqlite3_exec(lib->db, "BEGIN", NULL, NULL, NULL) == SQLITE_OK, "begin");
    check(sqlite3_exec(lib->db, "INSERT OR REPLACE INTO meta(key, value) VALUES('probe', '1')", NULL, NULL, NULL) == SQLITE_OK, "insert");
    check(sqlite3_exec(lib->db, "ROLLBACK", NULL, NULL, NULL) == SQLITE_OK, "rollback");
    g1 = sel_library_write_gen(lib);
    delta("a rollback", g0, g1, 0u);

    b.pin = 1;
    b.count = 3;
    b.now = 0;
    g0 = g1;
    p0 = sel_library_pin_gen(lib);
    memset(&reply, 0, sizeof reply);
    check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0, "pin batch");
    g1 = sel_library_write_gen(lib);
    delta("a pin batch", g0, g1, 1u);
    check(sel_library_pin_gen(lib) == p0 + 1u, "a pin batch moves pin_gen by 1");

    printf("hook_commits=%u pin_commits=%u\n", sel_library_write_gen(lib), sel_library_pin_gen(lib));
    check(sel_library_write_gen(lib) >= 3u, "the commit hook ran");
    sel_library_close(lib);
    if (fails == 0) printf("write_gen_ok\n");
    return fails != 0;
}
