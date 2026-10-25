/* I296: a scan writes each file's author tag into song_composers, so the Composers list is not empty after Add Folder.
 * The songdb sync replaces the tag row when it knows the song and leaves it when it does not; an untagged song stays out.
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
    check(strcmp(composers(lib, &c), "Lizardking=2;") == 0, "tag names the composer, untagged song stays out");

    scan(lib, "a.mod", "Lizardking"); /* a rescan keeps one row */
    check(strcmp(composers(lib, &c), "Lizardking=2;") == 0, "rescan is idempotent");

    apply(lib, "local:a.mod", "Dr Awesome\n"); /* songdb knows a.mod: its answer replaces the tag */
    check(strcmp(composers(lib, &c), "Dr Awesome=1;Lizardking=1;") == 0, "songdb replaces the tag row");

    scan(lib, "a.mod", "Lizardking"); /* a rescan does not bring the tag back over songdb data */
    check(strcmp(composers(lib, &c), "Dr Awesome=1;Lizardking=1;") == 0, "rescan keeps songdb data");

    apply(lib, "local:b.mod", ""); /* songdb does not know b.mod: the tag stays */
    check(strcmp(composers(lib, &c), "Dr Awesome=1;Lizardking=1;") == 0, "unknown to songdb keeps the tag");

    sel_library_close(lib);
    if (fails == 0) printf("library_scan_composers_ok\n");
    return fails == 0 ? 0 : 1;
}
