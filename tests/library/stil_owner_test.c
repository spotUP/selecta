/* I1276: the song panel resolves the STIL note of a modland-HVSC copy and of an hvsc.c64.org URL key through the `hvsc:` twin that owns the
 * stickers (nothing is copied). Entry point: sel_library_song_panel. A non-HVSC modland song gets none; an hvsc: song keeps its own. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "db_internal.h"
#include "source.h"
#include <stdio.h>
#include <string.h>

static int fails;

/* The hvsc plugin registers this id in the real app; without it sel_song_key_of would read `hvsc:...` as a local path. */
static const sel_source g_hvsc = {.id = "hvsc", .name = "HVSC", .home = "", .about = "test stand-in"};

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

/* The value of the panel row `label`, or NULL. */
static const char* row(const char* out, uint32_t rows, const char* label) {
    uint32_t i;

    for (i = 0; i < rows; i++) {
        const char* v = out + strlen(out) + 1u;
        if (strcmp(out, label) == 0) return v;
        out = v + strlen(v) + 1u;
    }
    return NULL;
}

static const char* stil(sel_library* lib, const char* key, char* out, uint32_t cap) {
    uint32_t rows = 0;

    if (!sel_library_song_panel(lib, key, out, cap, &rows)) return NULL;
    return row(out, rows, "STIL note");
}

int main(void) {
    sel_library* lib = sel_library_open(sel_host_default(), sel_machine_fake("a060"), 0, ":memory:");
    static char out[4096];
    const char* v;
    char own[200];

    sel_source_register_builtin();
    sel_source_register(&g_hvsc);
    check(lib != NULL, "open");
    if (lib == NULL) return 1;
    check(sqlite3_exec(lib->db,
                       "INSERT INTO songs(key, source, path, name, md5, format) VALUES"
                       "('hvsc:MUSICIANS/M/Miami/Miami_Vice.sid','hvsc','MUSICIANS/M/Miami/Miami_Vice.sid','Miami Vice','','sid'),"
                       "('modland:pub/modules/HVSC/MUSICIANS/M/Miami/Miami_Vice.sid','modland','pub/modules/HVSC/MUSICIANS/M/Miami/Miami_Vice.sid','Miami Vice','','sid'),"
                       "('modland:pub/modules/Protracker/x/y.mod','modland','pub/modules/Protracker/x/y.mod','y','','mod')",
                       NULL, NULL, NULL) == SQLITE_OK, "fixture");
    check(sel_library_sticker_set(lib, "hvsc:MUSICIANS/M/Miami/Miami_Vice.sid", "stil.comment", "Theme of the show") == 1, "sticker");

    v = stil(lib, "hvsc:MUSICIANS/M/Miami/Miami_Vice.sid", out, sizeof out);
    check(v != NULL && strcmp(v, "Theme of the show") == 0, "hvsc: song keeps its own note");
    v = stil(lib, "modland:pub/modules/HVSC/MUSICIANS/M/Miami/Miami_Vice.sid", out, sizeof out);
    check(v != NULL && strcmp(v, "Theme of the show") == 0, "modland HVSC copy returns the hvsc twin's note");
    v = stil(lib, "http://hvsc.c64.org/download/C64Music/MUSICIANS/M/Miami/Miami_Vice.sid", out, sizeof out);
    check(v != NULL && strcmp(v, "Theme of the show") == 0, "hvsc.c64.org URL key returns the twin's note");
    v = stil(lib, "modland:pub/modules/Protracker/x/y.mod", out, sizeof out);
    check(v == NULL, "non-HVSC modland song has no STIL");
    v = stil(lib, "modland:pub/modules/HVSC/MUSICIANS/Z/None/None.sid", out, sizeof out);
    check(v == NULL, "HVSC copy without a twin note has none");

    sel_stil_owner_key("http://hvsc.c64.org/download/C64Music/A/B%20C%281%29.sid", own, sizeof own);
    check(strcmp(own, "hvsc:A/B C(1).sid") == 0, "percent-encoded URL path decodes");
    sel_library_close(lib);
    if (fails == 0) printf("stil_owner_ok\n");
    return fails != 0;
}
