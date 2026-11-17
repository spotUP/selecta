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

    {
        char c[1024] = "";
        uint32_t r = 0;
        CHECK(sel_deepsid_csdb_rows("{\"csdbId\":7,\"releases\":[{\"id\":123,\"name\":\"Tune Pack\",\"screenshot\":\"/x.png\"},"
                                    "{\"id\":124,\"name\":\"Demo\",\"group\":\"Censor\",\"year\":1990,\"type\":\"C64 Music\"}]}", c, sizeof c, &r) == 2 && r == 2);
        CHECK(strcmp(get(c, "CSDb release"), "Tune Pack (csdb.dk/release/?id=123)") == 0);
        {
            const char* v1 = c + strlen(c) + 1;
            const char* l2 = v1 + strlen(v1) + 1;
            CHECK(strcmp(l2 + strlen(l2) + 1, "Demo, Censor, 1990, C64 Music (csdb.dk/release/?id=124)") == 0);
        }
    }
    rows = 0;
    CHECK(sel_deepsid_rows("{\"id\":1,\"tags\":[],\"placings\":[],\"gb64\":null}", e, sizeof e, &rows) == 0 && rows == 0 && e[0] == '\0');
    CHECK(sel_deepsid_rows(FULL, e, sizeof e, &rows) <= 3); /* a full buffer drops rows, never overruns */
    if (failures == 0) printf("library_deepsid_rows_ok\n");
    return failures != 0;
}
