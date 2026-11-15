/* I1401: which Demozoo query a song of any source asks. Modland keeps key=; every other source asks by title + artist and
 * asks nothing without both.
 * I1267: a song with a machine adds &platform=<family>, and only a production whose "families" holds it is read. */
#include "db.h"

#include <stdio.h>
#include <string.h>

#include "selecta/platform.h"

static int failures;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL line %d: %s\n", __LINE__, #cond);         \
            failures++;                                            \
        }                                                          \
    } while (0)

/* Two productions of one song: an Amiga one first (earliest), then a C64 one. */
static const char* const TWO =
    "{\"match\":true,\"productions\":["
    "{\"production_id\":1,\"families\":[\"amiga\"],\"screenshots\":[{\"url\":\"http://x/amiga.png\"}],\"video_url\":\"\"},"
    "{\"production_id\":2,\"families\":[\"c64\",\"atari\"],\"screenshots\":[{\"url\":\"http://x/c64.png\"}],\"video_url\":\"v\"}]}";
static const char* const OLD_SERVER =
    "{\"match\":true,\"productions\":[{\"production_id\":1,\"screenshots\":[{\"url\":\"http://x/amiga.png\"}],\"video_url\":\"\"}]}";

int main(void) {
    char q[256];
    const char* end = NULL;
    const char* p;

    CHECK(sel_dz_query("modland:pub/modules/Protracker/Skope/a b.mod", NULL, NULL, NULL, q, sizeof q) && strncmp(q, "key=modland", 11) == 0);
    CHECK(sel_dz_query("hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid", "Commando", "Rob Hubbard", NULL, q, sizeof q) && strncmp(q, "title=Commando&author=Rob", 25) == 0);
    CHECK(strchr(q, ' ') == NULL);
    CHECK(strstr(q, "platform") == NULL);
    CHECK(sel_dz_query("hvsc:a", "Turrican II", "Chris Huelsbeck", "c64", q, sizeof q) && strstr(q, "&platform=c64") != NULL);
    CHECK(sel_dz_query("modland:pub/modules/Protracker/Skope/a b.mod", NULL, NULL, "amiga", q, sizeof q) && strstr(q, "platform") == NULL);
    CHECK(!sel_dz_query("local:/m/x.mod", "Tune", "", NULL, q, sizeof q));
    CHECK(!sel_dz_query("local:/m/x.mod", "", "Artist", NULL, q, sizeof q));
    CHECK(!sel_dz_query("aminet:mods/a.lha", NULL, NULL, NULL, q, sizeof q));
    CHECK(!sel_dz_query("hvsc:a", "Commando", "Rob Hubbard", NULL, q, 8));

    CHECK(strcmp(sel_dz_family(SEL_PLATFORM_FAMILY_COMMODORE_64), "c64") == 0);
    CHECK(strcmp(sel_dz_family(SEL_PLATFORM_FAMILY_AMIGA), "amiga") == 0);
    CHECK(sel_dz_family(SEL_PLATFORM_FAMILY_MUSIC) == NULL);
    CHECK(sel_dz_family(SEL_PLATFORM_FAMILY_PC_TRACKER) == NULL);

    p = sel_dz_pick(TWO, "c64", &end);
    CHECK(p != NULL && strstr(p, "c64.png") != NULL && end != NULL && strncmp(end, "\"video_url\":\"v\"", 15) == 0);
    p = sel_dz_pick(TWO, "amiga", &end);
    CHECK(p != NULL && strstr(p, "amiga.png") != NULL && end != NULL && end < strstr(TWO, "\"production_id\":2"));
    p = sel_dz_pick(TWO, NULL, &end);
    CHECK(p != NULL && strstr(p, "amiga.png") != NULL); /* no machine: the first production, as before */
    CHECK(sel_dz_pick(TWO, "zx", &end) == NULL);
    CHECK(sel_dz_pick(OLD_SERVER, "c64", &end) == NULL); /* an older server sends no families: no image */
    CHECK(sel_dz_pick(OLD_SERVER, NULL, &end) != NULL);
    if (failures == 0) printf("library_dz_query_ok\n");
    return failures != 0;
}
