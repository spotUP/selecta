/* I1440l2: the non-drawing part of the CSDb release thumbnails: row id, page URL, artwork key, ids of a ds_csdb answer. */
#include "db.h"

#include <selecta/uimodel/csdb.h>

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
    const char* js = "{\"csdbId\":7,\"releases\":[{\"id\":101,\"name\":\"A\",\"screenshot\":\"x\"},{\"id\":202,\"name\":\"B\",\"screenshot\":\"y\"}]}";
    char out[256] = "", url[64], key[40];
    uint32_t rows = 0;
    int ids[6];

    CHECK(sel_deepsid_csdb_ids(js, ids, 6) == 2 && ids[0] == 101 && ids[1] == 202);
    CHECK(sel_deepsid_csdb_ids("{\"csdbId\":7}", ids, 6) == 0);
    CHECK(sel_deepsid_csdb_ids(NULL, ids, 6) == 0);
    CHECK(sel_deepsid_csdb_rows(js, out, sizeof out - 2, &rows) == 2); /* the row text carries the id the UI reads back */
    CHECK(sel_csdb_row_id(out + strlen(out) + 1) == 101);
    CHECK(sel_csdb_row_id("Plain text") == 0 && sel_csdb_row_id(NULL) == 0);
    sel_csdb_url(202, url, sizeof url);
    CHECK(strcmp(url, "https://csdb.dk/release/?id=202") == 0);
    sel_csdb_art_key(202, key, sizeof key);
    CHECK(strcmp(key, "csdb-release:202") == 0);
    {   /* I1468: a SID's CSDb "used in" list holds other productions; only the tune's own release passes */
        const char* used =
            "{\"csdbId\":10336,\"releases\":[{\"id\":5551,\"name\":\"Aural Mystery 2\",\"group\":\"Softkiller-Crew\",\"year\":1987,\"type\":\"One-File Demo\",\"screenshot\":\"a\"},"
            "{\"id\":3274,\"name\":\"Last Ninja\",\"group\":\"Blues Muz'\",\"year\":2001,\"type\":\"Music Collection\",\"screenshot\":\"b\"},"
            "{\"id\":9483,\"name\":\"Speed\",\"group\":\"Blues Muz'\",\"year\":2003,\"type\":\"Music\",\"screenshot\":\"c\"},"
            "{\"id\":7001,\"name\":\"Other Demo\",\"group\":\"Nobody\",\"year\":2001,\"type\":\"Demo\",\"screenshot\":\"d\"},"
            "{\"id\":7002,\"name\":\"The Last Ninja!\",\"group\":\"Zed\",\"year\":1987,\"type\":\"Demo\",\"screenshot\":\"e\"}]}";
        const char* remix = "{\"title\":\"Last Ninja 1 Remix\",\"copyright\":\"2001 Onslaught/Blues Muz'\",\"csdbType\":\"sid\",\"csdbId\":10336,\"stil\":\"\"}";
        const char* game = "{\"title\":\"The Last Ninja\",\"copyright\":\"1987 System 3\",\"csdbType\":\"sid\",\"csdbId\":10336,\"stil\":\"[from the C64 game The Last Ninja]\"}";
        const char* uri = "{\"title\":\"Uridium\",\"copyright\":\"1986 Hewson\",\"csdbType\":\"sid\",\"csdbId\":10336}";
        const char* rel = "{\"title\":\"X\",\"copyright\":\"1990 Y\",\"csdbType\":\"release\",\"csdbId\":124}";

        CHECK(sel_deepsid_csdb_own(remix, used, ids, 6) == 1 && ids[0] == 3274); /* same year and group; 2003 "Speed" is another year */
        CHECK(sel_deepsid_csdb_own(game, used, ids, 6) == 1 && ids[0] == 7002); /* year 1987 and name equals the STIL game; Aural Mystery 2 has neither */
        CHECK(sel_deepsid_csdb_own(uri, used, ids, 6) == 0);
        CHECK(sel_deepsid_csdb_own(rel, used, ids, 6) == 1 && ids[0] == 124);
        CHECK(sel_deepsid_csdb_own(NULL, used, ids, 6) == 0 && sel_deepsid_csdb_own("{}", used, ids, 6) == 0);
    }
    if (failures == 0) printf("library_deepsid_csdb_thumb_ok\n");
    return failures != 0;
}
