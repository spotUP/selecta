/* I678a: the Your Library item list (design 2026-10-29-desktop-library-architecture.md section 2), driven through the engine
 * entry sel_engine_library_items. library_items_calls in sel_engine_stats is the sentinel that the call reached the library.
 * Fixture: P playlists, A albums, R artists, C composers (with the Unknown group), S non-local sources. With no chip the list
 * holds 3 pinned rows + P + A + R + C + S; a kind mask keeps only its kinds; a window pages with the whole total. */
#include <selecta/engine.h>
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

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void sql(sel_library* lib, const char* text) {
    char* err = NULL;

    if (sqlite3_exec(lib->db, text, NULL, NULL, &err) != SQLITE_OK) {
        printf("FAIL sql: %s (%s)\n", text, err != NULL ? err : "?");
        g_fail++;
    }
}

static int64_t scalar(sel_library* lib, const char* text) {
    sqlite3_stmt* st = NULL;
    int64_t v = -1;

    if (sqlite3_prepare_v2(lib->db, text, -1, &st, NULL) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) v = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return v;
}

static const sel_engine_library_item* find(const sel_engine_library_item* rows, uint32_t n, uint32_t kind, const char* name) {
    uint32_t i;

    for (i = 0; i < n; i++) {
        if (rows[i].kind == kind && strcmp(rows[i].name, name) == 0) return &rows[i];
    }
    return NULL;
}

static void quiet(void* ctx, const sel_message* evt) {
    (void)ctx;
    (void)evt;
}

static uint32_t calls(sel_engine* e) {
    sel_engine_stats s;

    sel_engine_stats_get(e, &s);
    return s.library_items_calls;
}

int main(void) {
    const sel_host* host = sel_host_default();
    char dir[] = "/tmp/selecta_libitems_XXXXXX";
    char db[256];
    static sel_engine_library_item all[64], part[8];
    sel_engine_config cfg;
    sel_engine* e;
    sel_library* lib;
    uint32_t n, total = 0, p, a, r, c, s, before, i;
    const sel_engine_library_item* it;

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

    sql(lib,
        "INSERT INTO songs(key, source, path, name, artist, album, added) VALUES"
        "('local:a.mod','local','a.mod','a','Ann','Alb1',100),('local:b.mod','local','b.mod','b','Ann','Alb1',200),"
        "('local:c.mod','local','c.mod','c','Bob','Alb2',300),('modland:d.mod','modland','d.mod','d','Bob',NULL,400);"
        "INSERT INTO albums(name, artist) VALUES('Alb1','Ann'),('Alb2','Bob'),('Alb3','Cy');"
        "INSERT INTO song_composers(song_key, canon, display) VALUES('local:a.mod','x','X'),('local:b.mod','x','X'),"
        "('local:c.mod','y','Y');"
        "INSERT INTO playlists(name) VALUES('Mix'),('Chill');"
        "INSERT INTO playlist_entries(playlist_id, pos, key) VALUES(1,0,'local:a.mod'),(1,1,'local:b.mod');"
        "INSERT INTO liked(key, at) VALUES('local:a.mod',50);"
        "INSERT INTO history(key, played_at) VALUES('local:a.mod',500),('local:c.mod',700),('local:a.mod',600);");

    /* Counts of each kind, measured from the tables that produce them. */
    p = (uint32_t)scalar(lib, "SELECT COUNT(*) FROM playlists");
    a = (uint32_t)scalar(lib, "SELECT COUNT(*) FROM albums");
    r = (uint32_t)scalar(lib, "SELECT COUNT(*) FROM (SELECT artist FROM songs WHERE artist<>'' UNION SELECT artist FROM albums WHERE artist<>'')");
    c = (uint32_t)scalar(lib, "SELECT COUNT(*) FROM composers") + (uint32_t)scalar(lib, "SELECT COUNT(*) FROM composer_unknown WHERE n>0");
    s = (uint32_t)scalar(lib, "SELECT COUNT(*) FROM source_songs WHERE source<>'local' AND n>0");
    CHECK(p == 2u && a == 3u && r == 3u && c == 3u && s == 1u); /* the fixture itself */

    /* No chip: the whole list. */
    before = calls(e);
    n = sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 0, all, 64u, &total);
    CHECK(calls(e) == before + 1u); /* sentinel: the engine wrapper reached the library */
    CHECK(total == 3u + p + a + r + c + s);
    CHECK(total == 15u);
    CHECK(n == total);
    CHECK(all[0].kind == SEL_LIB_ITEM_LIKED && all[1].kind == SEL_LIB_ITEM_RECENT && all[2].kind == SEL_LIB_ITEM_LOCAL);
    CHECK(all[0].pinned && all[1].pinned && all[2].pinned && !all[3].pinned);
    for (i = 1; i < n; i++) CHECK(all[i - 1].kind <= all[i].kind); /* grouped by kind, pinned first */

    it = find(all, n, SEL_LIB_ITEM_LIKED, "Liked Songs");
    CHECK(it != NULL && it->songs == 1u && it->last_played == 50u);
    it = find(all, n, SEL_LIB_ITEM_RECENT, "Recently Played");
    CHECK(it != NULL && it->songs == 2u && it->last_played == 700u); /* distinct songs, newest play */
    it = find(all, n, SEL_LIB_ITEM_LOCAL, "Local Files");
    CHECK(it != NULL && it->songs == 3u);
    it = find(all, n, SEL_LIB_ITEM_PLAYLIST, "Mix");
    CHECK(it != NULL && it->id == 1u && it->songs == 2u && strcmp(it->sub, "You") == 0 && it->last_played == 600u);
    it = find(all, n, SEL_LIB_ITEM_PLAYLIST, "Chill");
    CHECK(it != NULL && it->songs == 0u); /* an empty playlist still lists */
    it = find(all, n, SEL_LIB_ITEM_ALBUM, "Alb1");
    CHECK(it != NULL && it->songs == 2u && strcmp(it->sub, "Ann") == 0 && it->added == 200u && it->last_played == 600u &&
          it->art_id == (uint32_t)scalar(lib, "SELECT id FROM albums WHERE name='Alb1'"));
    it = find(all, n, SEL_LIB_ITEM_ALBUM, "Alb3");
    CHECK(it != NULL && it->songs == 0u);
    it = find(all, n, SEL_LIB_ITEM_ARTIST, "Ann");
    CHECK(it != NULL && it->songs == 2u);
    it = find(all, n, SEL_LIB_ITEM_ARTIST, "Cy");
    CHECK(it != NULL && it->songs == 0u); /* an album artist without songs */
    it = find(all, n, SEL_LIB_ITEM_COMPOSER, "X");
    CHECK(it != NULL && it->songs == 2u && strcmp(it->key, "x") == 0);
    it = find(all, n, SEL_LIB_ITEM_COMPOSER, "Unknown composer");
    CHECK(it != NULL && it->songs == 1u);
    it = NULL;
    for (i = 0; i < n; i++) {
        if (all[i].kind == SEL_LIB_ITEM_SOURCE) it = &all[i];
    }
    CHECK(it != NULL && it->songs == 1u && strcmp(it->key, "modland") == 0 && it->name[0] != '\0'); /* name = registry display name */
    CHECK(find(all, n, SEL_LIB_ITEM_SOURCE, "local") == NULL); /* Local Files is the pinned row */

    /* Kind masks. */
    n = sel_engine_library_items(e, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_ALBUM), 0, all, 64u, &total);
    CHECK(n == a && total == a);
    for (i = 0; i < n; i++) CHECK(all[i].kind == SEL_LIB_ITEM_ALBUM);
    n = sel_engine_library_items(e, SEL_LIB_ITEM_PINNED_MASK, 0, all, 64u, &total);
    CHECK(n == 3u && total == 3u);
    n = sel_engine_library_items(e, SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_PLAYLIST) | SEL_LIB_ITEM_BIT(SEL_LIB_ITEM_COMPOSER), 0, all, 64u, &total);
    CHECK(n == p + c && total == p + c);
    CHECK(sel_engine_library_items(e, 0u, 0, all, 64u, &total) == 0u && total == 0u); /* an empty mask names nothing */

    /* Paging: a window of 4 rows from row 12 returns the last 3 of the full list, with the whole total. */
    n = sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 0, all, 64u, &total);
    CHECK(n == 15u);
    n = sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 12u, part, 4u, &total);
    CHECK(n == 3u && total == 15u);
    CHECK(strcmp(part[0].name, all[12].name) == 0 && part[0].kind == all[12].kind);
    CHECK(strcmp(part[2].name, all[14].name) == 0);
    n = sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 0, part, 5u, &total);
    CHECK(n == 5u && total == 15u); /* the window is capped, the total is not */

    /* I678c: LIBRARY_TOUCH through the protocol line writes the opened table; Recents data and Creator per kind come back with the rows. */
    {
        char pool[256], line[128];
        sel_engine_stats st;
        const sel_engine_library_item* alb;

        sel_engine_stats_get(e, &st);
        CHECK(st.lib_touch_calls == 0u);
        CHECK(sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 0, all, 64u, &total) == 15u);
        for (i = 0; i < 15u; i++) CHECK(all[i].opened == 0u); /* nothing opened yet */
        alb = find(all, 15u, SEL_LIB_ITEM_ALBUM, "Alb2");
        CHECK(alb != NULL && strcmp(alb->creator, "Bob") == 0); /* an album's creator is its artist */
        it = find(all, 15u, SEL_LIB_ITEM_ARTIST, "Cy");
        CHECK(it != NULL && strcmp(it->creator, "Cy") == 0); /* an artist is their own creator */
        it = find(all, 15u, SEL_LIB_ITEM_PLAYLIST, "Mix");
        CHECK(it != NULL && strcmp(it->creator, "You") == 0);
        snprintf(line, sizeof line, "LIBRARY_TOUCH %u \"%s\"", (unsigned)SEL_LIB_ITEM_ALBUM, alb->key);
        CHECK(sel_engine_send_line(e, line, pool, sizeof pool) == 0);
        (void)sel_engine_pump(e, quiet, NULL);
        sel_engine_stats_get(e, &st);
        CHECK(st.lib_touch_calls == 1u); /* sentinel: the engine command ran and wrote */
        CHECK(sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 0, all, 64u, &total) == 15u);
        alb = find(all, 15u, SEL_LIB_ITEM_ALBUM, "Alb2");
        CHECK(alb != NULL && alb->opened > 1000000u); /* stamped with the clock */
        it = find(all, 15u, SEL_LIB_ITEM_ALBUM, "Alb1");
        CHECK(it != NULL && it->opened == 0u && it->last_played == 600u); /* only the touched item moved */
        /* An artist key is its name, a composer key its canon; replacing keeps one row. */
        CHECK(sel_library_touch(lib, SEL_LIB_ITEM_ARTIST, "Cy", 900) == 0);
        CHECK(sel_library_touch(lib, SEL_LIB_ITEM_ARTIST, "Cy", 950) == 0);
        CHECK(sel_library_touch(lib, SEL_LIB_ITEM_COMPOSER, "y", 40) == 0);
        CHECK(scalar(lib, "SELECT COUNT(*) FROM opened") == 3);
        CHECK(sel_engine_library_items(e, SEL_LIB_ITEM_ALL, 0, all, 64u, &total) == 15u);
        it = find(all, 15u, SEL_LIB_ITEM_ARTIST, "Cy");
        CHECK(it != NULL && it->opened == 950u);
        it = find(all, 15u, SEL_LIB_ITEM_COMPOSER, "Y");
        CHECK(it != NULL && it->opened == 40u);
    }

    sel_engine_destroy(e);
    sel_library_close(lib);
    if (g_fail != 0) {
        printf("library_items_test: %d failure(s)\n", g_fail);
        return 1;
    }
    printf("library_items_ok\n");
    return 0;
}
