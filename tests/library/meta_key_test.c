/* I1465a (design 3, 7): sel_meta_key routes: SID family, md5 group, unique title + composer twin on the same platform family, own key. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include "source.h"
#include <stdio.h>
#include <string.h>

static int fails;

static void expect(sqlite3* db, const char* key, int route, const char* meta, const char* what) {
    char out[SEL_SONG_KEY_MAX];
    int r = sel_meta_key(db, key, out, sizeof out);

    if (r != route || strcmp(out, meta) != 0) {
        printf("FAIL %s: route %d key '%s'\n", what, r, out);
        fails++;
    }
}

int main(void) {
    sel_library* lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, ":memory:");
    sqlite3_stmt* st = NULL;
    int cached = 0;

    if (lib == NULL) return 1;
    if (sqlite3_exec(lib->db,
                     "INSERT INTO songs(key, source, path, name, artist, md5, format) VALUES"
                     "('hvsc:GAMES/A/Ace.sid','hvsc','GAMES/A/Ace.sid','Ace','','','sid'),"
                     "('local:/m/C64Music/GAMES/A/Ace.sid','local','/m/C64Music/GAMES/A/Ace.sid','Ace','','','sid'),"
                     "('local:/m/h1.mod','local','/m/h1.mod','Axel F','Harold Faltermeyer','','mod'),"
                     "('modland:pub/modules/Protracker/Harold Faltermeyer/axel f.mod','modland','pub/modules/Protracker/Harold Faltermeyer/axel f.mod','axel_f','Faltermeyer Harold','','mod'),"
                     "('local:/m/solo.mod','local','/m/solo.mod','Solo Tune','Some One','','mod'),"
                     "('local:/m/t1.mod','local','/m/t1.mod','Triple','Same Guy','','mod'),"
                     "('local:/m/t2.mod','local','/m/t2.mod','Triple','Same Guy','','mod'),"
                     "('local:/m/t3.mod','local','/m/t3.mod','Triple','Same Guy','','mod'),"
                     "('local:/m/o1.mod','local','/m/o1.mod','Other Tune','Guy A','','mod'),"
                     "('local:/m/o2.mod','local','/m/o2.mod','Other Tune','Guy B','','mod'),"
                     "('local:/m/e1.mod','local','/m/e1.mod','Echo','Eve','','mod'),"
                     "('local:/m/e2.xm','local','/m/e2.xm','Echo','Eve','','xm'),"
                     "('local:/m/m1.mod','local','/m/m1.mod','Hash A','','abc123','mod'),"
                     "('local:/m/m2.mod','local','/m/m2.mod','Hash B','','abc123','mod')",
                     NULL, NULL, NULL) != SQLITE_OK) {
        printf("FAIL fixture\n");
        return 1;
    }
    expect(lib->db, "hvsc:GAMES/A/Ace.sid", SEL_META_SID, "hvsc:GAMES/A/Ace.sid", "hvsc row is its own SID key");
    expect(lib->db, "local:/m/C64Music/GAMES/A/Ace.sid", SEL_META_SID, "hvsc:GAMES/A/Ace.sid", "local SID by suffix");
    expect(lib->db, "local:/m/h1.mod", SEL_META_TWIN, "local:/m/h1.mod", "twin: lower key (local < modland)");
    expect(lib->db, "modland:pub/modules/Protracker/Harold Faltermeyer/axel f.mod", SEL_META_TWIN, "local:/m/h1.mod", "twin agrees from the other copy");
    expect(lib->db, "local:/m/solo.mod", SEL_META_OWN, "local:/m/solo.mod", "no twin");
    expect(lib->db, "local:/m/t1.mod", SEL_META_OWN, "local:/m/t1.mod", "three copies: no unique twin");
    expect(lib->db, "local:/m/o1.mod", SEL_META_OWN, "local:/m/o1.mod", "same title, other composer");
    expect(lib->db, "local:/m/e1.mod", SEL_META_OWN, "local:/m/e1.mod", "same title, other platform family");
    expect(lib->db, "local:/m/m2.mod", SEL_META_HASH, "local:/m/m1.mod", "md5 group lowest key");
    expect(lib->db, "local:/m/none.mod", SEL_META_OWN, "local:/m/none.mod", "key with no row");
    sqlite3_prepare_v2(lib->db, "SELECT count(*) FROM song_meta_key WHERE key = 'local:/m/h1.mod' AND route = 3", -1, &st, NULL);
    if (sqlite3_step(st) == SQLITE_ROW) cached = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    if (cached != 1) {
        printf("FAIL cache row\n");
        fails++;
    }
    sel_library_close(lib);
    if (fails != 0) return 1;
    printf("meta_key_ok\n");
    return 0;
}
