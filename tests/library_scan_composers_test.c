/* I296: a scan writes each file's author tag into song_composers, so the Composers list is not empty after Add Folder.
 * The songdb sync replaces the tag row when it knows the song and leaves it when it does not; an untagged song stays out of the
 * named composers and lands in the "Unknown composer" group (I353).
 * Drives the same SEL_LIB_REQ_SCAN_BATCH request the folder scan sends. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

typedef struct list_ctx {
    char text[512];
    uint32_t rows;
} list_ctx;

static void on_composer(void* ctx, const char* canon, const char* display, uint32_t songs) {
    list_ctx* c = ctx;
    size_t n = strlen(c->text);

    (void)canon;
    snprintf(c->text + n, sizeof c->text - n, "%s=%u;", display, (unsigned)songs);
    c->rows++;
}

static int order_ok = 1;
static int order_rows;

static void on_composer_order(void* ctx, const char* canon, const char* display, uint32_t songs) {
    static char prev[64];

    (void)ctx;
    (void)canon;
    (void)songs;
    if (strncmp(display, "Composer ", 9) == 0) {
        if (prev[0] != '\0' && strcmp(prev, display) >= 0) order_ok = 0;
        snprintf(prev, sizeof prev, "%s", display);
    }
    order_rows++;
}

static const char* composers(sel_library* lib, list_ctx* c) {
    memset(c, 0, sizeof *c);
    check(sel_library_composer_list(lib, on_composer, c) >= 0, "composer list");
    return c->text;
}

/* I528: the list the maintained composers table gives must equal the GROUP BY it replaced, computed here from the base tables. */
static void same_as_group_by(sel_library* lib, const char* what) {
    static const char* const sql =
        "SELECT disp, n FROM ("
        " SELECT MIN(display) AS disp, COUNT(DISTINCT song_key) AS n, 0 AS last, canon FROM song_composers GROUP BY canon"
        " UNION ALL SELECT 'Unknown composer', COUNT(*), 1, '(unknown)'"
        "  FROM songs WHERE NOT EXISTS (SELECT 1 FROM song_composers c WHERE c.song_key = songs.key) HAVING COUNT(*) > 0)"
        " ORDER BY last, disp, canon";
    sqlite3_stmt* st = NULL;
    list_ctx a, b;
    char msg[96];
    size_t n = 0;

    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    check(sqlite3_prepare_v2(lib->db, sql, -1, &st, NULL) == SQLITE_OK, "group by prepares");
    while (st != NULL && sqlite3_step(st) == SQLITE_ROW && n < sizeof a.text) {
        n = strlen(a.text);
        snprintf(a.text + n, sizeof a.text - n, "%s=%d;", (const char*)sqlite3_column_text(st, 0), sqlite3_column_int(st, 1));
    }
    sqlite3_finalize(st);
    check(sel_library_composer_list(lib, on_composer, &b) >= 0, "composer list");
    snprintf(msg, sizeof msg, "table list equals GROUP BY %s", what);
    check(strcmp(a.text, b.text) == 0, msg);
    if (strcmp(a.text, b.text) != 0) printf("  group by: %s\n  table:    %s\n", a.text, b.text);
}

static void scan(sel_library* lib, const char* path, const char* artist) {
    sel_scan_row row;
    sel_scan_batch b;
    sel_message reply;

    memset(&row, 0, sizeof row);
    row.source = "local";
    row.path = path;
    row.format = "mod";
    row.artist = artist;
    row.name = path;
    memset(&b, 0, sizeof b);
    b.rows = &row;
    b.count = 1;
    b.now = 1;
    check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0, "scan batch");
}

static void apply(sel_library* lib, const char* key, const char* authors) {
    sel_genre_op op;

    memset(&op, 0, sizeof op);
    op.op = SEL_SONGDB_APPLY;
    op.key = key;
    op.value = authors;
    check(sel_genre_call(lib, &op, NULL) >= 0, "songdb apply");
}

int main(void) {
    sel_library* lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, ":memory:");
    list_ctx c;

    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    check(composers(lib, &c)[0] == '\0', "empty before the scan");

    scan(lib, "a.mod", "Lizardking");
    scan(lib, "b.mod", "Lizardking");
    scan(lib, "c.mod", "");
    check(strcmp(composers(lib, &c), "Lizardking=2;Unknown composer=1;") == 0, "tag names the composer, untagged song is the Unknown composer group");

    scan(lib, "a.mod", "Lizardking"); /* a rescan keeps one row */
    check(strcmp(composers(lib, &c), "Lizardking=2;Unknown composer=1;") == 0, "rescan is idempotent");

    apply(lib, "local:a.mod", "Dr Awesome\n"); /* songdb knows a.mod: its answer replaces the tag */
    check(strcmp(composers(lib, &c), "Dr Awesome=1;Lizardking=1;Unknown composer=1;") == 0, "songdb replaces the tag row");

    scan(lib, "a.mod", "Lizardking"); /* a rescan does not bring the tag back over songdb data */
    check(strcmp(composers(lib, &c), "Dr Awesome=1;Lizardking=1;Unknown composer=1;") == 0, "rescan keeps songdb data");

    apply(lib, "local:b.mod", ""); /* songdb does not know b.mod: the tag stays */
    check(strcmp(composers(lib, &c), "Dr Awesome=1;Lizardking=1;Unknown composer=1;") == 0, "unknown to songdb keeps the tag");


    same_as_group_by(lib, "after inserts");
    apply(lib, "local:b.mod", "Zed\nDr Awesome\n"); /* a composer change on a song: one spelling goes, two come */
    same_as_group_by(lib, "after a composer change");
    apply(lib, "local:b.mod", "Zed\n");
    same_as_group_by(lib, "after a second composer change");
    check(sqlite3_exec(lib->db, "DELETE FROM song_composers WHERE song_key = 'local:a.mod'", NULL, NULL, NULL) == SQLITE_OK,
          "drop composer rows");
    same_as_group_by(lib, "after the last composer row of a song went (Unknown grows)");
    check(sqlite3_exec(lib->db, "DELETE FROM songs WHERE key = 'local:c.mod'", NULL, NULL, NULL) == SQLITE_OK, "delete a song");
    same_as_group_by(lib, "after deleting an Unknown song");
    check(sqlite3_exec(lib->db, "DELETE FROM songs WHERE key = 'local:b.mod'", NULL, NULL, NULL) == SQLITE_OK, "delete a composed song");
    check(sqlite3_exec(lib->db, "DELETE FROM song_composers WHERE song_key = 'local:b.mod'", NULL, NULL, NULL) == SQLITE_OK, "drop its rows");
    same_as_group_by(lib, "after deleting a composed song");
    scan(lib, "b.mod", "Lizardking");
    scan(lib, "c.mod", "");
    same_as_group_by(lib, "after the songs return");

    /* I528: the whole list is ONE library request, whatever the composer count (it was one request, each re-running the full
     * GROUP BY with a growing OFFSET, per composer). Rows stay in display order. */
    {
        enum { MANY = 300 };
        char path[32], artist[32];
        uint32_t before, requests;
        int i, n;

        for (i = 0; i < MANY; i++) {
            snprintf(path, sizeof path, "m%03d.mod", i);
            snprintf(artist, sizeof artist, "Composer %03d", (i * 7) % MANY);
            scan(lib, path, artist);
        }
        before = lib->served;
        memset(&c, 0, sizeof c);
        n = sel_library_composer_list(lib, on_composer_order, &c);
        requests = lib->served - before;
        same_as_group_by(lib, "after 300 inserts");
        check(n >= MANY, "composer list row count");
        check(requests == 1u, "composer list is one library request");
        check(order_ok == 1 && order_rows == n, "composer list in display order");
    }

    sel_library_close(lib);
    if (fails == 0) printf("library_scan_composers_ok\n");
    return fails == 0 ? 0 : 1;
}
