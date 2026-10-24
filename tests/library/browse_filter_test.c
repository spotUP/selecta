/* Regression (I218i): the folder browser listed files that are not songs, and selecting one failed to play.
 * It must list folders plus files whose format Selecta recognises (extension or Amiga prefix, any case),
 * playlists and archives; everything else is hidden. The scan's own browse (songs_only 0) still sees all. */
#include "local.h"

#include <selecta/host.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void touch(const char* dir, const char* name) {
    char p[1024];
    FILE* f;

    snprintf(p, sizeof p, "%s/%s", dir, name);
    f = fopen(p, "wb");
    if (f != NULL) {
        fputs("x", f);
        fclose(f);
    }
}

static int has(const sel_browse* b, const char* name) {
    return sel_browse_find(b, name) != b->count;
}

int main(void) {
    char dir[] = "/tmp/selecta_browse_filter_XXXXXX";
    char sub[1024];
    sel_browse b;
    const sel_host* host = sel_host_default();

    CHECK(mkdtemp(dir) != NULL);
    snprintf(sub, sizeof sub, "%s/Folder", dir);
    CHECK(mkdir(sub, 0755) == 0);
    touch(dir, "readme.txt");
    touch(dir, "cover.jpg");
    touch(dir, "noextension");
    touch(dir, "song.mod");
    touch(dir, "LOUD.XM");
    touch(dir, "mod.tune");
    touch(dir, "MDAT.Intro");
    touch(dir, "list.m3u");
    touch(dir, "pack.lha");

    CHECK(sel_browse_init(&b, host, 64u, 4096u));
    b.songs_only = 1;
    CHECK(sel_browse_list(&b, dir));
    CHECK(!has(&b, "readme.txt")); /* unknown file hidden */
    CHECK(!has(&b, "cover.jpg"));
    CHECK(!has(&b, "noextension"));
    CHECK(has(&b, "song.mod")); /* known extension shown */
    CHECK(has(&b, "LOUD.XM")); /* upper case extension */
    CHECK(has(&b, "mod.tune")); /* Amiga prefix name */
    CHECK(has(&b, "MDAT.Intro")); /* upper case prefix */
    CHECK(has(&b, "Folder")); /* folder shown */
    CHECK(has(&b, "list.m3u")); /* playlist stays */
    CHECK(has(&b, "pack.lha")); /* archive stays */

    b.songs_only = 0; /* the library scan probes content, so it keeps every file */
    CHECK(sel_browse_list(&b, dir));
    CHECK(has(&b, "readme.txt"));
    sel_browse_free(&b, host);

    CHECK(sel_format_known_file("a.MOD") && sel_format_known_file("p61.x") && !sel_format_known_file("a.txt") &&
          !sel_format_known_file("mod") && !sel_format_known_file(""));

    if (g_fail == 0) {
        printf("browse_filter_ok\n");
    }
    return g_fail != 0;
}
