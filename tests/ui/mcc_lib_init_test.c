/* I685t: up-mcc.library applies the ENV:UP-MCC/theme tokens lazily: library open (up_lib_init) only arms, the first token read from a
 * client (UP_ThemeTokens) loads the file once, with no UP_ThemeReload call. Host: the same lib/support_lib.c through sel_mcc_static;
 * the ENV file is a path argument. Without the lazy load the first read sees the dark defaults and this fails. */
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

    /* armed but not yet read: library open did no file I/O, the store still holds the defaults (read through the store, not
     * UP_ThemeTokens, which is the lazy trigger) */
    up_lib_init(argv[1]);
    CHECK(memcmp(mui_theme_tokens(), def.token, sizeof def.token) == 0, "up_lib_init touched the store (file I/O at open)");
    CHECK(up_store_lazy_pending() == 1, "up_lib_init did not arm the lazy load");

    /* the first token read from a client: ENV tokens in force, no UP_ThemeReload / UP_ThemeLoad before it */
    memcpy(before, UP_ThemeTokens(), sizeof before);
    CHECK(memcmp(before, light.token, sizeof light.token) == 0, "first token read does not see the ENV theme tokens");
    CHECK(mui_theme_requests() == 0, "lazy load counted as a theme request");
    CHECK(up_store_lazy_pending() == 0, "once flag not set after the first read");

    /* once: a later change of the store is not undone by another read */
    mui_theme_tokens_set(def.token);
    CHECK(memcmp(UP_ThemeTokens(), def.token, sizeof def.token) == 0, "second read reloaded the file");

    /* an explicit reload makes the pending lazy load moot (missing ENV file: the defaults) */
    up_lib_init("/nonexistent/i685t/theme");
    CHECK(UP_ThemeReload() == 0, "reload failed");
    CHECK(up_store_lazy_pending() == 0, "UP_ThemeReload did not set the once flag");
    CHECK(memcmp(UP_ThemeTokens(), def.token, sizeof def.token) == 0, "missing ENV file did not give the defaults");
    printf("mcc-lib-init OK\n");
    return 0;
}
