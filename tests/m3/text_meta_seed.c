/* I1465t helper: seeds a library db for text-ui-meta-panel. The played song `argv[2]` and an unplayed twin (same title + composer, lower key) are two
 * songs rows; the stills and the video link are stored under the meta key of the played song (the twin's key), not under its own key. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include <stdio.h>
#include <string.h>

static int put(sqlite3* db, int album, const char* kind, const char* data) {
    sqlite3_stmt* st = NULL;
    int ok;

    if (sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO artwork(album_id, kind, data) VALUES(?1, ?2, ?3)", -1, &st, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(st, 1, album);
    sqlite3_bind_text(st, 2, kind, -1, SQLITE_STATIC);
    sqlite3_bind_blob(st, 3, data, (int)strlen(data), SQLITE_STATIC);
    ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

int main(int argc, char** argv) {
    sel_library* lib;
    char key[1024], mk[1024], sql[2048];
    int id;

    if (argc < 3) return 2;
    lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, argv[1]);
    if (lib == NULL) return 1;
    snprintf(key, sizeof key, "local:%s", argv[2]);
    snprintf(sql, sizeof sql,
             "INSERT OR REPLACE INTO songs(key, source, path, name, artist, md5, format) VALUES"
             "('%s','local','%s','Twin Tune','Twin Artist','','mod'),"
             "('local:/0twin.mod','local','/0twin.mod','Twin Tune','Twin Artist','','mod')", key, argv[2]);
    if (sqlite3_exec(lib->db, sql, NULL, NULL, NULL) != SQLITE_OK) return 1;
    if (sel_meta_key(lib->db, key, mk, sizeof mk) != SEL_META_TWIN || strcmp(mk, "local:/0twin.mod") != 0) {
        printf("FAIL meta key '%s'\n", mk);
        return 1;
    }
    id = sel_art_dz_id(mk);
    if (!put(lib->db, id, "cover", "cover-bytes") || !put(lib->db, id, "dz1", "still-bytes") || !put(lib->db, id, "dz_key", mk) ||
        !put(lib->db, id, "dz_video", "https://x.test/demo")) {
        printf("FAIL art rows\n");
        return 1;
    }
    sel_library_close(lib);
    printf("text_meta_seed_ok\n");
    return 0;
}
