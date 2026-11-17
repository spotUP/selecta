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
    if (failures == 0) printf("library_deepsid_csdb_thumb_ok\n");
    return failures != 0;
}
