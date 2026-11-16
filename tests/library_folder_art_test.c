/* I1401c: a local song whose folder holds cover.jpg (or a single image of any case) gets that image as its cover, found by the
 * same song-level lookup the Demozoo stills use (kick -> job -> artwork under sel_art_dz_id). No network: the folder hit comes first. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
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

static void put_file(const char* dir, const char* name, const void* data, size_t n) {
    char p[512];
    FILE* f;

    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (f != NULL) {
        fwrite(data, 1, n, f);
        fclose(f);
    }
}

static int cover_of(sel_library* lib, const char* key, uint8_t* out, uint32_t cap) {
    sel_art_op op;
    sel_message reply;

    memset(&op, 0, sizeof op);
    op.op = SEL_ART_FIND;
    op.ref = key;
    sel_library_art_op(lib, &op, &reply);
    if (reply.args[0].v.i == 0) return 0;
    memset(&op, 0, sizeof op);
    op.op = SEL_ART_GET;
    op.album = reply.args[0].v.i;
    op.kind = "cover";
    op.data = out;
    op.cap = cap;
    sel_library_art_op(lib, &op, &reply);
    return reply.args[0].v.i;
}

static void add_song(sel_library* lib, const char* dir) {
    char key[400], sql[900];

    snprintf(key, sizeof key, "local:%s/song.mod", dir);
    snprintf(sql, sizeof sql, "INSERT INTO songs(key, source, path, name) VALUES('%s','local','%s/song.mod','song')", key, dir);
    sqlite3_exec(lib->db, sql, NULL, NULL, NULL);
}

int main(void) {
    const sel_host* host = sel_host_default();
    char root[] = "/tmp/selecta_folderart_XXXXXX";
    char a[300], b[300], dbp[320], ka[400], kb[400], cmd[400];
    static const uint8_t jpg[16] = {0xFF, 0xD8, 0xFF, 0xE0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    static const uint8_t png[16] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    uint8_t got[64];
    sel_library* lib;
    int n;

    if (mkdtemp(root) == NULL) return 2;
    snprintf(a, sizeof a, "%s/a", root);
    snprintf(b, sizeof b, "%s/b", root);
    snprintf(dbp, sizeof dbp, "%s/lib.db", root);
    mkdir(a, 0700);
    mkdir(b, 0700);
    put_file(a, "song.mod", "x", 1);
    put_file(a, "Cover.JPG", jpg, sizeof jpg);
    put_file(a, "other.png", png, sizeof png);
    put_file(b, "song.mod", "x", 1);
    put_file(b, "Booklet.PNG", png, sizeof png); /* the only image of its folder */
    setenv("SELECTA_ART_DZ_URL", "http://127.0.0.1:1", 1);
    lib = sel_library_open(host, sel_machine_probe(host), 0, dbp);
    check(lib != NULL, "open");
    if (lib == NULL) return 1;

    snprintf(ka, sizeof ka, "local:%s/song.mod", a);
    snprintf(kb, sizeof kb, "local:%s/song.mod", b);
    add_song(lib, a);
    add_song(lib, b);
    check(cover_of(lib, ka, got, sizeof got) == 0, "no cover before the lookup");
    sel_library_demozoo_kick(lib, ka);
    sel_library_scrape_wait(lib);
    n = cover_of(lib, ka, got, sizeof got);
    check(n == (int)sizeof jpg && memcmp(got, jpg, sizeof jpg) == 0, "cover.jpg wins over the other image");

    sel_library_demozoo_kick(lib, kb);
    sel_library_scrape_wait(lib);
    n = cover_of(lib, kb, got, sizeof got);
    check(n == (int)sizeof png && memcmp(got, png, sizeof png) == 0, "a single image of any name is the cover");

    sel_library_close(lib);
    if (fails == 0) printf("library_folder_art_ok\n");
    snprintf(cmd, sizeof cmd, "rm -rf %s", root);
    return system(cmd) != 0 || fails != 0;
}
