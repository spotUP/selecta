/* I1440g: sel_deepsid_key maps every SID song to `hvsc:<path>` by route. With a db path as argv[1] it instead prints, for that (copied!) db,
 * how many SID songs per source get a key and by which route. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include "source.h"
#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

static void expect(sqlite3* db, const char* src, const char* path, const char* md5, int route, const char* key, const char* what) {
    char out[SEL_SONG_KEY_MAX];
    int r = sel_deepsid_key(db, src, path, md5, out, sizeof out);

    check(r == route && strcmp(out, key) == 0, what);
}

static int measure(const char* file) {
    sqlite3* db = NULL;
    sqlite3_stmt* st = NULL;
    long n[3][5] = {{0}};
    const char* const srcs[3] = {"hvsc", "modland", "other"};
    int g;

    if (sqlite3_open_v2(file, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) return 1;
    sqlite3_prepare_v2(db, "SELECT source, path, md5 FROM songs WHERE lower(path) LIKE '%.sid' OR lower(path) LIKE '%.psid'", -1, &st, NULL);
    while (sqlite3_step(st) == SQLITE_ROW) {
        const char* s = (const char*)sqlite3_column_text(st, 0);
        char out[SEL_SONG_KEY_MAX];

        g = strcmp(s, "hvsc") == 0 ? 0 : strcmp(s, "modland") == 0 ? 1 : 2;
        n[g][sel_deepsid_key(db, s, (const char*)sqlite3_column_text(st, 1), (const char*)sqlite3_column_text(st, 2), out, sizeof out)]++;
    }
    sqlite3_finalize(st);
    for (g = 0; g < 3; g++) {
        printf("%-8s none=%ld hvsc=%ld modland=%ld md5=%ld suffix=%ld\n", srcs[g], n[g][0], n[g][1], n[g][2], n[g][3], n[g][4]);
    }
    sqlite3_close(db);
    return 0;
}

int main(int argc, char** argv) {
    sel_library* lib;

    if (argc > 1) return measure(argv[1]);
    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, ":memory:");
    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    check(sqlite3_exec(lib->db,
                       "INSERT INTO songs(key, source, path, name, md5, format) VALUES"
                       "('hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid','hvsc','MUSICIANS/H/Hubbard_Rob/Commando.sid','Commando','6d019ecb','sid'),"
                       "('hvsc:GAMES/A/Ace.sid','hvsc','GAMES/A/Ace.sid','Ace','','sid')",
                       NULL, NULL, NULL) == SQLITE_OK, "fixture");
    expect(lib->db, "hvsc", "GAMES/A/Ace.sid", "", SEL_DEEPSID_HVSC, "hvsc:GAMES/A/Ace.sid", "hvsc row");
    expect(lib->db, "modland", "pub/modules/HVSC/GAMES/A/Ace.sid", "", SEL_DEEPSID_MODLAND, "hvsc:GAMES/A/Ace.sid", "modland HVSC path");
    expect(lib->db, "modland", "pub/modules/RealSID/x/y.sid", "", SEL_DEEPSID_NONE, "", "non-HVSC modland has no key");
    expect(lib->db, "local", "/m/rips/commando_rip.sid", "6d019ecb", SEL_DEEPSID_MD5, "hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid", "md5 route");
    expect(lib->db, "local", "/m/C64Music/GAMES/A/Ace.sid", "ffff", SEL_DEEPSID_SUFFIX, "hvsc:GAMES/A/Ace.sid", "suffix route after md5 miss");
    expect(lib->db, "local", "/m/xGAMES/A/Ace.sid", "", SEL_DEEPSID_NONE, "", "suffix needs a path-segment boundary");
    expect(lib->db, "local", "/m/C64Music/GAMES/Z/None.sid", "", SEL_DEEPSID_NONE, "", "suffix without an hvsc row misses");
    expect(lib->db, "local", "/m/a.sid", NULL, SEL_DEEPSID_NONE, "", "NULL md5, no tail");
    sel_library_close(lib);
    if (fails == 0) printf("deepsid_key_ok\n");
    return fails != 0;
}
