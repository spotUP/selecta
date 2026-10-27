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
        check(n == MANY + 3, "composer list row count");
        check(requests == 1u, "composer list is one library request");
        check(order_ok == 1 && order_rows == MANY + 3, "composer list in display order");
    }

    sel_library_close(lib);
    if (fails == 0) printf("library_scan_composers_ok\n");
    return fails == 0 ? 0 : 1;
}
