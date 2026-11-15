/* I1401: which Demozoo query a song of any source asks. Modland keeps key=; every other source asks by title + artist and
 * asks nothing without both. */
#include "db.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL line %d: %s\n", __LINE__, #cond);         \
            failures++;                                            \
        }                                                          \
    } while (0)

int main(void) {
    char q[256];

    CHECK(sel_dz_query("modland:pub/modules/Protracker/Skope/a b.mod", NULL, NULL, q, sizeof q) && strncmp(q, "key=modland", 11) == 0);
    CHECK(sel_dz_query("hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid", "Commando", "Rob Hubbard", q, sizeof q) && strncmp(q, "title=Commando&author=Rob", 25) == 0);
    CHECK(strchr(q, ' ') == NULL);
    CHECK(!sel_dz_query("local:/m/x.mod", "Tune", "", q, sizeof q));
    CHECK(!sel_dz_query("local:/m/x.mod", "", "Artist", q, sizeof q));
    CHECK(!sel_dz_query("aminet:mods/a.lha", NULL, NULL, q, sizeof q));
    CHECK(!sel_dz_query("hvsc:a", "Commando", "Rob Hubbard", q, 8));
    if (failures == 0) printf("library_dz_query_ok\n");
    return failures != 0;
}
