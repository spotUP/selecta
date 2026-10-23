/* Regression: apps/selecta browsed a start dir only when it began with "/", so
 * AmigaDOS paths such as "Work:Music" and "PROGDIR:" never browsed. */
#include "local.h"

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
    CHECK(sel_path_is_absolute_style("/music", 0));
    CHECK(sel_path_is_absolute_style("/music", 1));
    CHECK(sel_path_is_absolute_style("Work:Music", 1));
    CHECK(sel_path_is_absolute_style("PROGDIR:", 1));
    CHECK(!sel_path_is_absolute_style("Work:Music", 0));
    CHECK(!sel_path_is_absolute_style("music/a:b", 1)); /* ':' after a '/' is a file name */
    CHECK(!sel_path_is_absolute_style(":x", 1));
    CHECK(!sel_path_is_absolute_style("", 1));
    CHECK(!sel_path_is_absolute_style("rel/dir", 1));
    /* I138b: join and parent in both path styles */
    {
        char o[64];
        CHECK(sel_browse_join_style(o, sizeof o, "Work:", "Music", 1) && strcmp(o, "Work:Music") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "Work:Music", "Mods", 1) && strcmp(o, "Work:Music/Mods") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "Work:Music/", "Mods", 1) && strcmp(o, "Work:Music/Mods") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "Work:Music/Mods", "..", 1) && strcmp(o, "Work:Music") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "Work:Music", "..", 1) && strcmp(o, "Work:") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "Work:", "..", 1) && strcmp(o, "Work:") == 0);
        CHECK(!sel_browse_join_style(o, 8, "Work:Music", "Mods", 1));
        CHECK(sel_browse_is_root_style("Work:", 1) && !sel_browse_is_root_style("Work:Music", 1));
        CHECK(!sel_browse_is_root_style("Work:", 0) && sel_browse_is_root_style("/", 0));
        CHECK(strcmp(sel_browse_basename_style("Work:Music", 1), "Music") == 0);
        CHECK(strcmp(sel_browse_basename_style("Work:Music/Mods", 1), "Mods") == 0);
        CHECK(strcmp(sel_browse_basename_style("a:b", 0), "a:b") == 0);
        /* desktop unchanged */
        CHECK(sel_browse_join_style(o, sizeof o, "/", "x", 0) && strcmp(o, "/x") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "/a/b", "x", 0) && strcmp(o, "/a/b/x") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "/a/b", "..", 0) && strcmp(o, "/a") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "/a", "..", 0) && strcmp(o, "/") == 0);
        CHECK(sel_browse_join_style(o, sizeof o, "/", "..", 0) && strcmp(o, "/") == 0);
    }
    printf(g_fail == 0 ? "library_path_test: ok\n" : "library_path_test: %d failures\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
