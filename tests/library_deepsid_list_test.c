/* I1440m: the DeepSID list answers (tags, groups, players, folders, file lists) as rows (sel_deepsid_list_parse). */
#include "db.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                        \
    do {                                                   \
        if (!(cond)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #cond); \
            failures++;                                    \
        }                                                  \
    } while (0)

typedef struct got {
    int n;
    char f[8][4][96];
} got;

static void keep(void* ctx, const char* const* f) {
    got* g = (got*)ctx;
    int i;
    if (g->n >= 8) return;
    for (i = 0; i < 4; i++) snprintf(g->f[g->n][i], sizeof g->f[0][0], "%s", f[i]);
    g->n++;
}

int main(void) {
    got g;
    int r;

    memset(&g, 0, sizeof g);
    r = sel_deepsid_list_parse("tags", "[{\"id\":3,\"name\":\"Rock\",\"type\":\"genre\",\"count\":120},{\"id\":4,\"name\":\"Cover\",\"type\":\"origin\",\"count\":7}]", keep, &g);
    CHECK(r == 2 && g.n == 2);
    CHECK(strcmp(g.f[0][0], "Rock") == 0 && strcmp(g.f[0][1], "120") == 0 && strcmp(g.f[0][2], "genre") == 0 && strcmp(g.f[0][3], "3") == 0);

    memset(&g, 0, sizeof g);
    r = sel_deepsid_list_parse("groups", "[{\"id\":1,\"composer\":\"A\",\"group\":\"Fairlight\",\"path\":\"x\"},{\"id\":2,\"composer\":\"B\",\"group\":\"Fairlight\",\"path\":\"y\"},{\"id\":3,\"composer\":\"C\",\"group\":\"Maniacs\",\"path\":\"z\"}]", keep, &g);
    CHECK(g.n == 2 && strcmp(g.f[0][0], "Fairlight") == 0 && strcmp(g.f[0][1], "2") == 0 && strcmp(g.f[1][0], "Maniacs") == 0 && strcmp(g.f[1][1], "1") == 0);

    memset(&g, 0, sizeof g);
    r = sel_deepsid_list_parse("players", "[{\"id\":9,\"title\":\"Rob Hubbard\",\"developer\":\"Rob\",\"startYear\":1984}]", keep, &g);
    CHECK(r == 1 && strcmp(g.f[0][0], "Rob Hubbard") == 0 && strcmp(g.f[0][2], "Rob") == 0 && strcmp(g.f[0][3], "9") == 0);

    memset(&g, 0, sizeof g);
    r = sel_deepsid_list_parse("folders", "[{\"id\":1,\"path\":\"_High Voltage SID Collection/MUSICIANS\",\"name\":\"MUSICIANS\",\"type\":\"folder\",\"files\":0}]", keep, &g);
    CHECK(r == 1 && strcmp(g.f[0][0], "_High Voltage SID Collection/MUSICIANS") == 0 && strcmp(g.f[0][2], "MUSICIANS") == 0);

    memset(&g, 0, sizeof g);
    r = sel_deepsid_list_parse("files", "[{\"id\":5,\"path\":\"_High Voltage SID Collection/MUSICIANS/H/Hubbard_Rob/Commando.sid\",\"name\":\"Commando\",\"author\":\"Rob Hubbard\",\"player\":\"RH\",\"subtunes\":2}]", keep, &g);
    CHECK(r == 1 && strcmp(g.f[0][0], "hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid") == 0 && strcmp(g.f[0][1], "Commando") == 0 && strcmp(g.f[0][2], "Rob Hubbard") == 0);

    memset(&g, 0, sizeof g);
    r = sel_deepsid_list_parse("tunes", "{\"tunes\":[{\"fullname\":\"_High Voltage SID Collection/A/b.sid\",\"name\":\"b\",\"author\":\"x\"}],\"total\":1}", keep, &g);
    CHECK(r == 1 && strcmp(g.f[0][0], "hvsc:A/b.sid") == 0);

    memset(&g, 0, sizeof g);
    r = sel_deepsid_list_parse("tags", "[]", keep, &g);
    CHECK(r == 0 && g.n == 0);

    if (failures == 0) printf("library_deepsid_list_ok\n");
    return failures != 0;
}
