/* I1440k: the Song Information rows of a DeepSID ds_file answer (sel_deepsid_rows). */
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

static const char* get(const char* list, const char* label) {
    while (*list != '\0') {
        const char* v = list + strlen(list) + 1;
        if (strcmp(list, label) == 0) return v;
        list = v + strlen(v) + 1;
    }
    return NULL;
}

static const char* const FULL =
    "{\"placings\":[{\"eventId\":9,\"name\":\"C64 music\",\"releaseId\":1,\"place\":2,\"competition\":\"Assembly\",\"year\":1994},"
    "{\"eventId\":10,\"name\":\"Music\",\"place\":11,\"competition\":\"X\",\"year\":1995}],"
    "\"type\":\"PSID\",\"playerType\":\"Own\",\"loadAddr\":\"$0FFF\",\"initAddr\":\"$1000\",\"playAddr\":\"$1003\",\"stil\":null,\"gb64\":\"Commando\","
    "\"application\":\"SidTracker\",\"id\":42,\"player\":\"Rob_Hubbard\",\"sidModel\":\"6581\",\"clockSpeed\":\"PAL\","
    "\"tags\":[{\"name\":\"Rock\",\"type\":\"GENRE\"},{\"name\":\"Metal\",\"type\":\"genre\"},{\"name\":\"Cover\",\"type\":\"ORIGIN\"},{\"name\":\"Odd\",\"type\":null}]}";

int main(void) {
    char out[2048] = "STIL note\0from stil\0";
    char e[64] = "";
    uint32_t rows = 1, n;

    n = sel_deepsid_rows(FULL, out, sizeof out, &rows);
    CHECK(n == 11 && rows == 12);
    CHECK(strcmp(get(out, "STIL note"), "from stil") == 0); /* existing rows stay */
    CHECK(strcmp(get(out, "Genre tags"), "Rock, Metal") == 0);
    CHECK(strcmp(get(out, "Origin tags"), "Cover") == 0);
    CHECK(strcmp(get(out, "Other tags"), "Odd") == 0);
    CHECK(strcmp(get(out, "Player"), "Rob_Hubbard, Own") == 0);
    CHECK(strcmp(get(out, "Editor"), "SidTracker") == 0);
    CHECK(strcmp(get(out, "Chip"), "6581") == 0 && strcmp(get(out, "Clock"), "PAL") == 0);
    CHECK(strcmp(get(out, "Technical"), "Load $0FFF, init $1000, play $1003") == 0);
    CHECK(strcmp(get(out, "Compo placing"), "2nd at Assembly 1994, C64 music") == 0);
    CHECK(strcmp(get(out, "GB64 game"), "Commando") == 0);
    CHECK(get(out, "Production tags") == NULL);

    rows = 0;
    CHECK(sel_deepsid_rows("{\"id\":1,\"tags\":[],\"placings\":[],\"gb64\":null}", e, sizeof e, &rows) == 0 && rows == 0 && e[0] == '\0');
    CHECK(sel_deepsid_rows(FULL, e, sizeof e, &rows) <= 3); /* a full buffer drops rows, never overruns */
    {
        static const char* const COMP =
            "{\"found\":true,\"id\":7,\"fullname\":\"MUSICIANS/H/Hubbard_Rob\",\"name\":\"Rob Hubbard\",\"handles\":[\"Rob\",\"Hubbard\"],\"focus\":\"Musician\","
            "\"born\":\"1955-08-02\",\"died\":null,\"country\":\"United Kingdom\",\"notable\":\"Commando\",\"employment\":[{\"company\":\"Ariolasoft\",\"years\":\"1985\"}],"
            "\"affiliation\":\"Hotline\",\"csdbType\":\"scener\",\"csdbId\":1234,\"photo\":\"musicians_h_hubbard_rob.jpg\","
            "\"links\":[{\"name\":\"Homepage\",\"url\":\"https://example.org\"}],\"counts\":{\"files\":97,\"placings\":3,\"youtube\":12,\"collection\":\"MUSICIANS\"},"
            "\"tags\":[{\"name\":\"Rock\",\"type\":\"GENRE\"}]}";
        char c[2048];

        n = sel_deepsid_composer_rows(COMP, c, sizeof c);
        CHECK(n == 13);
        CHECK(strcmp(get(c, "Name"), "Rob Hubbard") == 0 && strcmp(get(c, "Handles"), "Rob, Hubbard") == 0);
        CHECK(strcmp(get(c, "Born"), "1955-08-02") == 0 && get(c, "Died") == NULL);
        CHECK(strcmp(get(c, "Country"), "United Kingdom") == 0 && strcmp(get(c, "Groups"), "Hotline") == 0);
        CHECK(strcmp(get(c, "Worked at"), "Ariolasoft, 1985") == 0 && strcmp(get(c, "Notable"), "Commando") == 0);
        CHECK(strcmp(get(c, "Link"), "Homepage: https://example.org") == 0);
        CHECK(strcmp(get(c, "Songs in HVSC"), "97") == 0 && strcmp(get(c, "Compo placings"), "3") == 0 && strcmp(get(c, "YouTube captures"), "12") == 0);
        CHECK(sel_deepsid_composer_rows("{\"found\":false}", c, sizeof c) == 0);
        CHECK(sel_deepsid_composer_rows(COMP, e, sizeof e) <= 3);
    }
    if (failures == 0) printf("library_deepsid_rows_ok\n");
    return failures != 0;
}
