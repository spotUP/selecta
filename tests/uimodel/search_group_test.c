/* I1019d: sel_uim_search_group (heading kinds 1..4) and sel_uim_search_all_line (SOURCE_SEARCH * "<q>"). */
#include <selecta/uimodel.h>
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

int main(void) {
    char buf[64];
    char tiny[8];
    CHECK(sel_uim_search_group("Songs (12)") == 1u);
    CHECK(sel_uim_search_group("Albums (3)") == 2u);
    CHECK(sel_uim_search_group("Artists (1)") == 3u);
    CHECK(sel_uim_search_group("From Modules.pl (40)") == 4u);
    CHECK(sel_uim_search_group("Searching") == 0u);
    CHECK(sel_uim_search_group("") == 0u);
    CHECK(sel_uim_search_group(NULL) == 0u);

    CHECK(sel_uim_search_all_line("chip", buf, sizeof buf) == strlen("SOURCE_SEARCH * \"chip\""));
    CHECK(strcmp(buf, "SOURCE_SEARCH * \"chip\"") == 0);
    CHECK(sel_uim_search_all_line("a\"b\\c", buf, sizeof buf) > 0u);
    CHECK(strcmp(buf, "SOURCE_SEARCH * \"a\\\"b\\\\c\"") == 0);
    CHECK(sel_uim_search_all_line(NULL, buf, sizeof buf) > 0u);
    CHECK(strcmp(buf, "SOURCE_SEARCH * \"\"") == 0);
    CHECK(sel_uim_search_all_line("x", tiny, sizeof tiny) == 0u);
    CHECK(sel_uim_search_line("chip", buf, sizeof buf) > 0u);
    CHECK(strcmp(buf, "LIBRARY_SEARCH \"chip\"") == 0);
    if (g_fail == 0) {
        printf("uim_search_group_ok\n");
    }
    return g_fail != 0;
}
