/* I263: a song key (a file path) crosses the home view whole. The library writes the home text, the view parses it
 * into row keys, and the play verb queues r->key as it stands, so the key must come out of the view byte for byte.
 * A 300-byte path was cut at 127 bytes by the old per-row key slot. Since I678j the Home song shelves list a song with a skipped play (Jump Back In);
 * a finished library song with no album has no card, so the plays here are skips. */
#include <selecta/host.h>
#include <selecta/machine.h>
#include "db.h"
#include "view.h"
#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL %s\n", what);
        fails++;
    }
}

/* I678j: the format is stored with each play, so a Modland module play files under the Modules chip, a stream under Music. Written through
 * sel_library_history_add (the store derives the format from the key), read back through the chip-filtered Home text. */
static int chip_has(sel_library* lib, uint32_t chip, const char* key) {
    static char text[65536];
    uint32_t lines = 0;
    text[0] = '\0';
    if (!sel_library_home_chip(lib, chip, 20, text, sizeof text, &lines)) return -1;
    return strstr(text, key) != NULL;
}

static void play_format_chips(const sel_host* host) {
    sel_library* lib = sel_library_open(host, sel_machine_fake("a060"), 0, ":memory:");
    const char* mod = "modland:Protracker/Jester/old.mod";
    const char* radio = "http://radio.example/stream";
    check(lib != NULL, "play format library opens");
    if (lib == NULL) return;
    check(sel_library_history_add(lib, mod, 100, 0), "play format: modland add");
    check(sel_library_history_add(lib, radio, 200, 0), "play format: stream add");
    check(chip_has(lib, SEL_HOME_CHIP_ALL, mod) == 1 && chip_has(lib, SEL_HOME_CHIP_ALL, radio) == 1, "play format: All holds both");
    check(chip_has(lib, SEL_HOME_CHIP_MODULES, mod) == 1, "play format: Modland .mod under Modules");
    check(chip_has(lib, SEL_HOME_CHIP_MODULES, radio) == 0, "play format: stream not under Modules");
    check(chip_has(lib, SEL_HOME_CHIP_MUSIC, radio) == 1, "play format: stream under Music");
    check(chip_has(lib, SEL_HOME_CHIP_MUSIC, mod) == 0, "play format: Modland .mod not under Music");
    check(chip_has(lib, SEL_HOME_CHIP_CHIPTUNES, mod) == 0, "play format: Modland .mod not under Chiptunes");
    sel_library_close(lib);
}

/* I486: a key near the limit plus a long multi-byte artist. The key must survive whole (activation opens it), the artist is cut
 * on a UTF-8 boundary to the room the key leaves. */
static void long_artist(const sel_host* host) {
    sel_library* lib = sel_library_open(host, sel_machine_fake("a060"), 0, ":memory:");
    sel_view v;
    sel_scan_row row;
    sel_scan_batch b;
    sel_message reply;
    char path[1200], key[1210], artist[200];
    uint32_t lines = 0, i, seen = 0;
    size_t al = 0;

    check(lib != NULL, "i486 library opens");
    if (lib == NULL) return;
    memset(&v, 0, sizeof v);
    check(sel_view_init(&v, host, 20u), "i486 view init");
    path[0] = '/';
    memset(path + 1, 'c', 1178);
    memcpy(path + 1179, ".mod", 5);
    snprintf(key, sizeof key, "local:%s", path); /* 1189 bytes */
    for (i = 0; i < 40u; i++) {
        artist[al++] = (char)0xC3; /* A with diaeresis, two bytes each: 80 bytes */
        artist[al++] = (char)0x84;
    }
    artist[al] = '\0';
    memset(&row, 0, sizeof row);
    row.source = "local";
    row.path = path;
    row.format = "mod";
    row.artist = artist;
    row.name = "Long Song";
    memset(&b, 0, sizeof b);
    b.rows = &row;
    b.count = 1;
    b.now = 1;
    check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0, "i486 scan");
    check(sel_library_history_add(lib, key, 100, 1), "i486 history add");
    check(sel_library_home(lib, 10, v.htext, SEL_VIEW_HOME_TEXT_MAX, &lines), "i486 home text");
    sel_view_set_home(&v, v.htext);
    for (i = 0; i < v.frow_n; i++) {
        if (v.frows[i].key != NULL && strcmp(v.frows[i].key, key) == 0) {
            const char* a = v.frows[i].artist;
            size_t n = a != NULL ? strlen(a) : 0u;
            seen++;
            check(a != NULL && n > 0u && n < al, "i486 artist kept but cut");
            check(n % 2u == 0u, "i486 artist cut on a UTF-8 boundary");
            check(v.frows[i].art_ref != NULL && strcmp(v.frows[i].art_ref, key) == 0, "i486 art_ref is the whole key");
        }
    }
    check(seen == 1u, "i486 long key with long artist keeps its key");
    sel_view_free(&v, host);
    sel_library_close(lib);
}

/* I678h2: a Home album tile carries albums.id, so the UI opens the album page by id and never matches names. Since I678j the tile is a
 * library item tile whose key is the id; the name and artist the tile draws stay as they were. */
static void album_id(const sel_host* host) {
    sel_library* lib = sel_library_open(host, sel_machine_fake("a060"), 0, ":memory:");
    sel_view v;
    sel_scan_row row;
    sel_scan_batch b;
    sel_message reply;
    sel_art_op art;
    uint8_t png[4] = {1, 2, 3, 4};
    uint32_t lines = 0, i, seen = 0;
    int64_t want = 0;

    check(lib != NULL, "i678h2 library opens");
    if (lib == NULL) return;
    memset(&v, 0, sizeof v);
    check(sel_view_init(&v, host, 20u), "i678h2 view init");
    memset(&row, 0, sizeof row);
    row.source = "local";
    row.path = "/m/one.mod";
    row.format = "mod";
    row.artist = "Art Ist";
    row.album = "Some Album";
    row.name = "One";
    memset(&b, 0, sizeof b);
    b.rows = &row;
    b.count = 1;
    b.now = 1;
    check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b, &reply) && reply.args[1].v.i == 0, "i678h2 scan");
    memset(&art, 0, sizeof art);
    art.op = SEL_ART_STORE;
    art.kind = "cover";
    art.data = png;
    art.len = sizeof png;
    art.name = "Some Album";
    art.artist = "Art Ist";
    check(sel_library_request(lib, SEL_LIB_REQ_ART, &art, &reply), "i678h2 albums row created");
    memset(&art, 0, sizeof art);
    art.op = SEL_ART_FIND;
    art.ref = "local:/m/one.mod";
    check(sel_library_request(lib, SEL_LIB_REQ_ART, &art, &reply), "i678h2 album id found");
    want = reply.args[0].v.i;
    check(want > 0, "i678h2 the albums row has an id");
    check(sel_library_history_add(lib, "local:/m/one.mod", 100, 0), "i678h2 history add");
    check(sel_library_home(lib, 10, v.htext, SEL_VIEW_HOME_TEXT_MAX, &lines), "i678h2 home text");
    sel_view_set_home(&v, v.htext);
    for (i = 0; i < v.frow_n; i++) {
        /* I678j: a Home album is a library item tile, "#4:<albums.id>\x1f<artist>\t<name>"; the key is the id. */
        if (v.frows[i].item_kind == SEL_LIB_ITEM_ALBUM + 1u && v.frows[i].key != NULL) {
            char id[24];
            snprintf(id, sizeof id, "%lld", (long long)want);
            seen++;
            check(strcmp(v.frows[i].key, id) == 0, "i678h2 album tile carries albums.id");
            check(strcmp(v.frows[i].text, "Some Album") == 0, "i678h2 album name kept");
            check(v.frows[i].artist != NULL && strcmp(v.frows[i].artist, "Art Ist") == 0, "i678h2 album artist kept");
        }
    }
    check(seen >= 1u, "i678h2 an album tile exists");
    sel_view_free(&v, host);
    sel_library_close(lib);
}

int main(void) {
    const sel_host* host = sel_host_default();
    sel_library* lib = sel_library_open(host, sel_machine_fake("a060"), 0, ":memory:");
    sel_view v;
    char key[512], key2[1200];
    uint32_t lines = 0, i, seen = 0, seen2 = 0, rows;

    check(lib != NULL, "library opens");
    if (lib == NULL) return 1;
    memset(&v, 0, sizeof v);
    check(sel_view_init(&v, host, 20u), "view init");
    memcpy(key, "local:/music/", 13);
    memset(key + 13, 'a', 283);
    memcpy(key + 296, ".mod", 5); /* 300 bytes of path, the tail is the extension the old cut dropped */
    memcpy(key2, "local:/", 7);
    memset(key2 + 7, 'b', 1180);
    memcpy(key2 + 1187, ".xm", 4); /* 1190 bytes: near the SEL_SONG_KEY_MAX limit */
    /* Home hides history of local: songs that are not in the library (I555), so both songs must be scanned in. */
    {
        sel_scan_row rows2[2];
        sel_scan_batch b2;
        sel_message reply2;
        memset(rows2, 0, sizeof rows2);
        rows2[0].source = "local";
        rows2[0].path = key + 6; /* key is "local:/music/..." */
        rows2[0].format = "mod";
        rows2[0].name = "Long One";
        rows2[1].source = "local";
        rows2[1].path = key2 + 6;
        rows2[1].format = "xm";
        rows2[1].name = "Long Two";
        memset(&b2, 0, sizeof b2);
        b2.rows = rows2;
        b2.count = 2;
        b2.now = 1;
        check(sel_library_request(lib, SEL_LIB_REQ_SCAN_BATCH, &b2, &reply2) && reply2.args[1].v.i == 0, "scan both songs");
    }
    check(sel_library_history_add(lib, key, 100, 1), "history add 300");
    check(sel_library_history_add(lib, key2, 200, 1), "history add 1190");
    check(sel_library_home(lib, 10, v.htext, SEL_VIEW_HOME_TEXT_MAX, &lines), "home text");
    rows = sel_view_set_home(&v, v.htext);
    check(rows >= 2u, "home entries counted");
    for (i = 0; i < v.frow_n; i++) {
        if (v.frows[i].key != NULL && strcmp(v.frows[i].key, key) == 0) seen++;
        if (v.frows[i].key != NULL && strcmp(v.frows[i].key, key2) == 0) seen2++;
    }
    check(seen == 1u, "300-byte key round-trips exactly");
    check(seen2 == 1u, "1190-byte key round-trips exactly");
    sel_view_free(&v, host);
    sel_library_close(lib);
    long_artist(host);
    album_id(host);
    play_format_chips(host);
    printf(fails ? "home-key: %d failure(s)\n" : "home-key: ok\n", fails);
    return fails ? 1 : 0;
}
