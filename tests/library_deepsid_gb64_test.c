/* I1465f (design D3a): the PHP-serialized `gb64` list of a DeepSID file answer: parse (bounds-robust), pick by game title, and the game-tune gate. */
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

int main(void) {
    const char* php = "a:3:{i:0;a:2:{s:4:\"name\";s:10:\"Uridium v2\";s:2:\"id\";s:5:\"11606\";}i:1;a:2:{s:4:\"name\";s:7:\"Uridium\";s:2:\"id\";s:4:\"2001\";}"
                      "i:2;a:2:{s:4:\"name\";s:13:\"Uridium Plus!\";s:2:\"id\";i:3002;}}";
    const char* game =
        "{\"title\":\"Uridium\",\"application\":\"RELEASE\",\"tags\":[{\"name\":\"Game\",\"type\":0}],"
        "\"gb64\":\"a:2:{i:0;a:2:{s:4:\\\"name\\\";s:10:\\\"Uridium v2\\\";s:2:\\\"id\\\";s:5:\\\"11606\\\";}i:1;a:2:{s:4:\\\"name\\\";s:7:\\\"Uridium\\\";s:2:\\\"id\\\";s:4:\\\"2001\\\";}}\"}";
    char buf[512];
    sel_gb64_entry e[8];
    uint32_t n, len;

    n = sel_deepsid_gb64_parse(php, e, 8);
    CHECK(n == 3 && e[0].id == 11606 && strcmp(e[0].name, "Uridium v2") == 0 && e[1].id == 2001 && e[2].id == 3002);
    CHECK(sel_deepsid_gb64_parse(php, e, 2) == 2); /* the cap holds */
    CHECK(sel_deepsid_gb64_parse(NULL, e, 8) == 0 && sel_deepsid_gb64_parse("", e, 8) == 0 && sel_deepsid_gb64_parse("N;", e, 8) == 0);
    CHECK(sel_deepsid_gb64_parse("a:0:{}", e, 8) == 0);
    /* every cut of a valid list parses without a read past the end, and never more than the full list */
    len = (uint32_t)strlen(php);
    for (n = 0; n < len; n++) {
        memcpy(buf, php, n);
        buf[n] = '\0';
        CHECK(sel_deepsid_gb64_parse(buf, e, 8) <= 3);
    }
    /* a length that lies: the entry before it stays, the bad one ends the list */
    CHECK(sel_deepsid_gb64_parse("a:2:{i:0;a:2:{s:4:\"name\";s:1:\"A\";s:2:\"id\";s:1:\"5\";}i:1;a:2:{s:4:\"name\";s:99:\"B\";s:2:\"id\";s:1:\"6\";}}", e, 8) == 1);
    CHECK(sel_deepsid_gb64_parse("a:1:{i:0;a:2:{s:4:\"name\";s:1:\"A\";s:2:\"id\";s:1:\"0\";}}", e, 8) == 0); /* id 0 is no entry */

    n = sel_deepsid_gb64_parse(php, e, 8);
    CHECK(sel_deepsid_gb64_pick(e, n, "URIDIUM") == 2001); /* exact, case-insensitive, beats the earlier v2 */
    CHECK(sel_deepsid_gb64_pick(e, n, "Uridium Plus") == 3002); /* prefix */
    CHECK(sel_deepsid_gb64_pick(e, n, "Nothing Alike") == 11606); /* no match: the first entry */
    CHECK(sel_deepsid_gb64_pick(e, n, NULL) == 11606);
    CHECK(sel_deepsid_gb64_pick(e, 0, "Uridium") == 0 && sel_deepsid_gb64_pick(NULL, 3, "Uridium") == 0);

    CHECK(sel_deepsid_gb64_id(game) == 2001); /* the escaped string of a real answer, matched on the file title */
    CHECK(sel_deepsid_gb64_id(NULL) == 0);
    strcpy(buf, game);
    memcpy(strstr(buf, "Game"), "Demo", 4);
    CHECK(sel_deepsid_gb64_id(buf) == 0); /* not tagged Game */
    strcpy(buf, game);
    memcpy(strstr(buf, "RELEASE"), "PREVIEW", 7);
    CHECK(sel_deepsid_gb64_id(buf) == 0); /* not a release */
    CHECK(sel_deepsid_gb64_id("{\"application\":\"RELEASE\",\"tags\":[{\"name\":\"Game\"}],\"gb64\":null}") == 0);

    if (failures == 0) printf("library_deepsid_gb64_ok\n");
    return failures == 0 ? 0 : 1;
}
