/* I263: a song key (a file path) crosses the home view whole. The library writes the home text, the view parses it
 * into row keys, and the play verb queues r->key as it stands, so the key must come out of the view byte for byte.
 * A 300-byte path was cut at 127 bytes by the old per-row key slot. */
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
    check(sel_library_history_add(lib, key, 100, 0), "history add 300");
    check(sel_library_history_add(lib, key2, 200, 0), "history add 1190");
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
    printf(fails ? "home-key: %d failure(s)\n" : "home-key: ok\n", fails);
    return fails ? 1 : 0;
}
