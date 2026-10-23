/* I193: the LRC parser, the current-line lookup and the .lrc path (src/library/lyrics.c). Pure functions. */
#include "lyrics.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            failures++;                                                   \
        }                                                                 \
    } while (0)

static sel_lrc lrc; /* 20 KB: static, not stack */

static void parse(const char* s) {
    (void)sel_lrc_parse(&lrc, s, strlen(s));
}

int main(void) {
    char path[64];

    parse("[ti:Title]\n[ar:Artist]\n[00:12.00]Third\n[00:04.00]First\n[00:08.5]Second\n[01:00]Last\n");
    CHECK(lrc.count == 4);
    CHECK(strcmp(sel_lrc_text(&lrc, 0), "First") == 0); /* sorted by time, header tags ignored */
    CHECK(lrc.ms[1] == 8500u && lrc.ms[3] == 60000u);
    CHECK(sel_lrc_index_at(&lrc, 3999u) == -1);
    CHECK(sel_lrc_index_at(&lrc, 4000u) == 0);
    CHECK(sel_lrc_index_at(&lrc, 12500u) == 2);
    CHECK(sel_lrc_index_at(&lrc, 999999u) == 3);
    CHECK(sel_lrc_text(&lrc, 4) == NULL);

    parse("[00:01.00][00:03.00]Chorus\r\n[00:02.123]Odd\r\n");
    CHECK(lrc.count == 3 && lrc.ms[0] == 1000u && lrc.ms[1] == 2123u && lrc.ms[2] == 3000u);
    CHECK(strcmp(sel_lrc_text(&lrc, 2), "Chorus") == 0);

    parse("[offset:+500]\n[00:02.00]Early\n");
    CHECK(lrc.count == 1 && lrc.ms[0] == 1500u); /* positive offset: the line appears earlier */

    parse("\xEF\xBB\xBF[00:01:50]Bom and colon fraction\n");
    CHECK(lrc.count == 1 && lrc.ms[0] == 1500u);

    parse("Just plain words\nwithout stamps\n[not a stamp]\n");
    CHECK(lrc.count == 0 && sel_lrc_index_at(&lrc, 5000u) == -1);

    parse("[00:05.00]\n[00:06.00]Words\n");
    CHECK(lrc.count == 2 && strcmp(sel_lrc_text(&lrc, 0), "") == 0); /* an empty line is an instrumental gap */

    CHECK(sel_lrc_path("dir/song.mod", path, sizeof path) && strcmp(path, "dir/song.lrc") == 0);
    CHECK(sel_lrc_path("dir.d/song", path, sizeof path) && strcmp(path, "dir.d/song.lrc") == 0);
    CHECK(sel_lrc_path("mod.song.it", path, sizeof path) && strcmp(path, "mod.song.lrc") == 0);
    CHECK(sel_lrc_path("DH0:Mods/tune.xm", path, sizeof path) && strcmp(path, "DH0:Mods/tune.lrc") == 0);
    CHECK(!sel_lrc_path("a/long/name.mod", path, 6));

    if (failures == 0) printf("lyrics_unit_ok\n");
    return failures == 0 ? 0 : 1;
}
