/* I706d: the About view model holds the credit text of the bundled modules. argv[1] = a folder of module folders
 * (plugins/replayers in the source tree: sunvox_lib/CREDITS.txt is the credit under test). */
#include <selecta/uimodel/about.h>
#include <stdio.h>
#include <string.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

int main(int argc, char** argv) {
    sel_uim_about a;
    sel_uim_credit c;
    uint32_t i, found = 0;
    CHECK(argc == 2);
    CHECK(strcmp(sel_uim_about_heading(), "About Selecta") == 0);
    CHECK(sel_uim_credit_parse("Title\r\nCopyright line\nLicence: MIT\n\nbody text\n", &c));
    CHECK(strcmp(c.title, "Title") == 0 && strcmp(c.copyright, "Copyright line") == 0 && strcmp(c.licence, "Licence: MIT") == 0);
    CHECK(!sel_uim_credit_parse("\n\n", &c));
    sel_uim_about_scan("/nonexistent-selecta-about-dir", &a);
    CHECK(a.count == 0u);
    sel_uim_about_scan(argc == 2 ? argv[1] : NULL, &a);
    for (i = 0; i < a.count; i++) {
        if (strcmp(a.credit[i].title, "Powered by SunVox (modular synth & tracker)") == 0) {
            found++;
            CHECK(strstr(a.credit[i].copyright, "Alexander Zolotov") != NULL);
            CHECK(strstr(a.credit[i].copyright, "WarmPlace.ru") != NULL);
            CHECK(strncmp(a.credit[i].licence, "Licence:", 8) == 0 && strstr(a.credit[i].licence, "MIT") != NULL);
        }
    }
    CHECK(found == 1u);
    if (g_fail == 0) {
        printf("uim_about_ok credits=%u\n", (unsigned)a.count);
    }
    return g_fail != 0;
}
