/* I1440o: the composer page header lines the text UI draws (sel_uim_about_line) over the real DeepSID composer rows (sel_deepsid_composer_rows). */
#include "db.h"

#include <selecta/uimodel/pages.h>
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
    static const char* const COMP =
        "{\"found\":true,\"id\":7,\"name\":\"Rob Hubbard\",\"handles\":[\"Rob\"],\"born\":\"1955-08-02\",\"country\":\"United Kingdom\","
        "\"links\":[{\"name\":\"Homepage\",\"url\":\"https://example.org\"},{\"name\":\"Wiki\",\"url\":\"https://example.org/w\"}],"
        "\"counts\":{\"files\":97,\"placings\":3,\"youtube\":12,\"collection\":\"MUSICIANS\"}}";
    char about[2048] = "";
    char l[40], v[300];
    uint32_t i, links = 0, n = sel_deepsid_composer_rows(COMP, about, sizeof about);

    CHECK(n > 4);
    CHECK(sel_uim_about_line(about, 0, 12, l, sizeof l, v, sizeof v) && strcmp(l, "Name") == 0 && strcmp(v, "Rob Hubbard") == 0);
    for (i = 0; sel_uim_about_line(about, i, 12, l, sizeof l, v, sizeof v); i++) {
        if (strncmp(l, "Link ", 5) == 0) {
            links++;
            CHECK(l[5] == (char)('0' + links));
        }
    }
    CHECK(links == 2);
    CHECK(i == n);
    CHECK(!sel_uim_about_line(about, 2, 2, l, sizeof l, v, sizeof v));
    CHECK(!sel_uim_about_line("", 0, 12, l, sizeof l, v, sizeof v));
    if (failures == 0) printf("uim_about_lines_ok\n");
    return failures != 0;
}
