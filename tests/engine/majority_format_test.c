/* I691c: the majority format (platform family) of an item's songs, through the real builders: the Your Library list
 * (sel_engine_library_items), a collection shelf (sel_library_collection) and the Home view (sel_library_home + sel_view_set_home).
 * Design: thoughts/shared/research/2026-10-29-i691-platform-covers.md section 4.
 * Fixture: Mixed = 1 mp3 first, then 3 mod (the minority song comes first, so first-song logic fails); Tied = 2 xm + 2 mod
 * (tie goes to the earlier family, Amiga before PC Tracker); Plain = mp3 + ogg (Music only when alone); Stray = 1 mp3 + 1 xm (Music
 * loses to any other family). library_items_calls is the sentinel that the call reached the library. */
#include <selecta/engine.h>
#include <selecta/gui/gui_genart.h>
#include <selecta/host.h>
#include <selecta/library_item.h>
#include <selecta/machine.h>
#include <selecta/registry.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db/db.h"
#include "db/db_internal.h"
#include "view.h"

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static uint32_t fam(const char* name_or_ext) { return sel_platform_of_ext(name_or_ext); }

typedef struct shelf_ctx {
    char fmt[4][16];
    char name[4][32];
    uint32_t n;
} shelf_ctx;

static void shelf_row(void* ctx, const char* const* f) {
    shelf_ctx* k = (shelf_ctx*)ctx;

    if (k->n >= 4u) return;
    snprintf(k->name[k->n], sizeof k->name[0], "%s", f[1]);
    snprintf(k->fmt[k->n], sizeof k->fmt[0], "%s", f[5]);
    k->n++;
}

static const sel_engine_library_item* find(const sel_engine_library_item* rows, uint32_t n, uint32_t kind, const char* name) {
    uint32_t i;

    for (i = 0; i < n; i++) {
        if (rows[i].kind == kind && strcmp(rows[i].name, name) == 0) return &rows[i];
    }
    return NULL;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_majfmt_XXXXXX";
    char db[256], *err = NULL;
    static sel_engine_library_item all[64];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    sel_engine_stats st0, st1;
    sel_view v;
    shelf_ctx sc;
    uint32_t n, total = 0, i, lines = 0;
    const sel_engine_library_item* it;
    int seen_tile = 0;

    if (mkdtemp(dir) == NULL) return 2;
    snprintf(db, sizeof db, "%s/lib.db", dir);
    CHECK(sel_registry_init(host) > 0);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db);
    CHECK(lib != NULL);
    memset(&cfg, 0, sizeof cfg);
    cfg.library = lib;
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    e = sel_engine_create(host, &cfg);
    CHECK(e != NULL);
    if (lib == NULL || e == NULL) return 2;

    CHECK(sqlite3_exec(lib->db,
                       "INSERT INTO songs(key, source, path, name, artist, album, added) VALUES"
                       "('local:m0.mp3','local','m0.mp3','m0','Ann','Mixed',1),('local:m1.mod','local','m1.mod','m1','Ann','Mixed',2),"
                       "('local:m2.mod','local','m2.mod','m2','Ann','Mixed',3),('local:m3.mod','local','m3.mod','m3','Ann','Mixed',4),"
                       "('local:t0.xm','local','t0.xm','t0','Bob','Tied',5),('local:t1.xm','local','t1.xm','t1','Bob','Tied',6),"
                       "('local:t2.mod','local','t2.mod','t2','Bob','Tied',7),('local:t3.mod','local','t3.mod','t3','Bob','Tied',8),"
                       "('local:p0.mp3','local','p0.mp3','p0','Cy','Plain',9),('local:p1.ogg','local','p1.ogg','p1','Cy','Plain',10),"
                       "('local:s0.mp3','local','s0.mp3','s0','Di','Stray',11),('local:s1.xm','local','s1.xm','s1','Di','Stray',12);"
                       "INSERT INTO albums(name, artist) VALUES('Mixed','Ann'),('Tied','Bob'),('Plain','Cy'),('Stray','Di');"
                       "INSERT INTO playlists(name) VALUES('Mod Mix');"
                       "INSERT INTO playlist_entries(playlist_id, pos, key) VALUES(1,0,'local:m0.mp3'),(1,1,'local:m1.mod'),"
                       "(1,2,'local:m2.mod');"
                       "INSERT INTO history(key, played_at) VALUES('local:m1.mod',500),('local:t2.mod',400);",
                       NULL, NULL, &err) == SQLITE_OK);

    /* 1. The Your Library list through the engine entry. */
    sel_engine_stats_get(e, &st0);
    n = sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 0, all, 64u, &total);
    sel_engine_stats_get(e, &st1);
    CHECK(st1.library_items_calls == st0.library_items_calls + 1u); /* sentinel: the call reached the library */
    CHECK(n >= 9u);
    it = find(all, n, SEL_LIB_ITEM_ALBUM, "Mixed");
    CHECK(it != NULL && it->format[0] != '\0' && fam(it->format) == SEL_PLATFORM_FAMILY_AMIGA); /* 3 mod beat 1 mp3 */
    it = find(all, n, SEL_LIB_ITEM_ALBUM, "Tied");
    CHECK(it != NULL && fam(it->format) == SEL_PLATFORM_FAMILY_AMIGA); /* 2 mod / 2 xm: Amiga before PC Tracker */
    it = find(all, n, SEL_LIB_ITEM_ALBUM, "Plain");
    CHECK(it != NULL && it->format[0] != '\0' && fam(it->format) == SEL_PLATFORM_FAMILY_MUSIC); /* Music when alone */
    it = find(all, n, SEL_LIB_ITEM_ALBUM, "Stray");
    CHECK(it != NULL && fam(it->format) == SEL_PLATFORM_FAMILY_PC_TRACKER); /* one tracker song beats a stray mp3 */
    it = find(all, n, SEL_LIB_ITEM_ARTIST, "Bob");
    CHECK(it != NULL && fam(it->format) == SEL_PLATFORM_FAMILY_AMIGA);
    it = find(all, n, SEL_LIB_ITEM_PLAYLIST, "Mod Mix");
    CHECK(it != NULL && fam(it->format) == SEL_PLATFORM_FAMILY_AMIGA);

    /* 2. A collection shelf: the artist's albums carry the album's format in field 5. */
    memset(&sc, 0, sizeof sc);
    CHECK(sel_library_collection(lib, SEL_LIB_ITEM_ARTIST, "Bob", SEL_COLL_ALBUMS, shelf_row, &sc) >= 0);
    CHECK(sc.n == 1u && strcmp(sc.name[0], "Tied") == 0 && sc.fmt[0][0] != '\0' && fam(sc.fmt[0]) == SEL_PLATFORM_FAMILY_AMIGA);
    memset(&sc, 0, sizeof sc);
    CHECK(sel_library_collection(lib, SEL_LIB_ITEM_ARTIST, "Ann", SEL_COLL_ALBUMS, shelf_row, &sc) >= 0);
    CHECK(sc.n == 1u && sc.fmt[0][0] != '\0' && fam(sc.fmt[0]) == SEL_PLATFORM_FAMILY_AMIGA);

    /* 3. Home: the album tile of the played album, through the library text and the view parser. */
    memset(&v, 0, sizeof v);
    CHECK(sel_view_init(&v, host, 20u));
    CHECK(sel_library_home(lib, 10, v.htext, SEL_VIEW_HOME_TEXT_MAX, &lines));
    sel_view_set_home(&v, v.htext);
    for (i = 0; i < v.frow_n; i++) {
        if (v.frows[i].album && v.frows[i].key != NULL && strcmp(v.frows[i].key, "Mixed") == 0) {
            seen_tile = 1;
            CHECK(v.frows[i].format != NULL && v.frows[i].format[0] != '\0' && fam(v.frows[i].format) == SEL_PLATFORM_FAMILY_AMIGA);
            CHECK(v.frows[i].art_id != 0u); /* the id tag before the format tag still parses */
        }
    }
    CHECK(seen_tile);

    sel_engine_destroy(e);
    sel_library_close(lib);
    printf(g_fail == 0 ? "majority_format_ok\n" : "majority_format_test: %d FAIL\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
