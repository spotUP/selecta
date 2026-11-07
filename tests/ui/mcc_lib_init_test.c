/* I685t: up-mcc.library open (up_lib_init, called by __UserLibInit) puts the ENV theme file's tokens into the store with no
 * UP_ThemeReload call, so shipped themes act before the prefs page ever saves. Host: the same lib/support_lib.c through
 * sel_mcc_static; the ENV file is a path argument (the library passes ENV:UP-MCC/theme). Without the theme step in up_lib_init the
 * tokens stay at the built-in dark defaults and this fails. */
#include "map/mui_theme.h"
#include "map/up_store.h"

#include <mcc/up.h>

#include <selecta/uimodel/theme.h>

#include <stdio.h>
#include <string.h>

#define CHECK(c, ...)                                  \
    do {                                               \
        if (!(c)) {                                    \
            fprintf(stderr, "FAIL line %d: ", __LINE__); \
            fprintf(stderr, __VA_ARGS__);              \
            fprintf(stderr, "\n");                     \
            return 1;                                  \
        }                                              \
    } while (0)

int main(int argc, char** argv) {
    txt_theme def, light;
    uint32_t before[SEL_GUI_TOKEN_COUNT];
    CHECK(argc == 2, "usage: mcc_lib_init_test <Light.theme>");
    txt_theme_default(&def);
    txt_theme_default(&light);
    CHECK(mui_theme_load(&light, NULL, argv[1]) == 1, "cannot load %s", argv[1]);
    CHECK(memcmp(def.token, light.token, sizeof def.token) != 0, "Light equals the defaults: test proves nothing");

    memcpy(before, UP_ThemeTokens(), sizeof before);
    CHECK(memcmp(before, def.token, sizeof before) == 0, "store does not start at the defaults");

    up_lib_init(argv[1]); /* the library open path; no UP_ThemeReload / UP_ThemeLoad anywhere in this file */
    CHECK(memcmp(UP_ThemeTokens(), light.token, sizeof light.token) == 0, "ENV theme tokens not in force after library init");
    CHECK(mui_theme_requests() == 0, "init counted as a theme request");

    /* a missing ENV file leaves the built-in defaults (a fresh install has no ENV:UP-MCC/theme) */
    up_lib_init("/nonexistent/i685t/theme");
    CHECK(memcmp(UP_ThemeTokens(), def.token, sizeof def.token) == 0, "missing ENV file did not give the defaults");
    printf("mcc-lib-init OK\n");
    return 0;
}
