/* Regression: apps/selecta browsed a start dir only when it began with "/", so
 * AmigaDOS paths such as "Work:Music" and "PROGDIR:" never browsed. */
#include "local.h"

#include <stdio.h>

static int g_fail;
#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

int main(void) {
    CHECK(sel_path_is_absolute_style("/music", 0));
    CHECK(sel_path_is_absolute_style("/music", 1));
    CHECK(sel_path_is_absolute_style("Work:Music", 1));
    CHECK(sel_path_is_absolute_style("PROGDIR:", 1));
    CHECK(!sel_path_is_absolute_style("Work:Music", 0));
    CHECK(!sel_path_is_absolute_style("music/a:b", 1)); /* ':' after a '/' is a file name */
    CHECK(!sel_path_is_absolute_style(":x", 1));
    CHECK(!sel_path_is_absolute_style("", 1));
    CHECK(!sel_path_is_absolute_style("rel/dir", 1));
    printf(g_fail == 0 ? "library_path_test: ok\n" : "library_path_test: %d failures\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
