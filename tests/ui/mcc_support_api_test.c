/* I685h: the up-mcc.library API (design 2026-11-06-mui-mcc-package.md section 8.4) through the static build, which compiles the same
 * lib/support_lib.c as the library. One entry point per row: tokens round trip, icon lookup, font file list, theme load error. */
#include "map/mui_icons.h"
#include "map/mui_theme.h"

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

int main(void) {
    uint32_t known[SEL_GUI_TOKEN_COUNT];
    for (uint32_t i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        known[i] = 0x102030u + i * 0x010101u;
    }
    /* tokens */
    CHECK(UP_ThemeSetTokens(known, SEL_GUI_TOKEN_COUNT) == 0, "set tokens refused");
    const ULONG* got = UP_ThemeTokens();
    CHECK(got != NULL, "tokens NULL");
    for (uint32_t i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        CHECK(got[i] == known[i], "token %u: %lx != %lx", (unsigned)i, (unsigned long)got[i], (unsigned long)known[i]);
    }
    CHECK((const void*)got == (const void*)mui_theme_tokens(), "UP_ThemeTokens is not the store pointer");
    /* a short array overlays the front, the rest stays */
    ULONG two[2] = {0xAABBCCu, 0x112233u};
    CHECK(UP_ThemeSetTokens(two, 2) == 0, "short set refused");
    got = UP_ThemeTokens();
    CHECK(got[0] == 0xAABBCCu && got[1] == 0x112233u && got[2] == known[2], "short overlay wrong");
    CHECK(UP_ThemeSetTokens(NULL, 3) != 0, "NULL array accepted");
    /* icons */
    CHECK(mui_icon_index("play") >= 0, "play missing from the baked set");
    CHECK(UP_IconFind((STRPTR) "play") == mui_icon_index("play"), "icon index differs");
    CHECK(UP_IconFind((STRPTR) "no-such-icon") == -1, "unknown icon not -1");
    CHECK(UP_IconFind(NULL) == -1, "NULL icon not -1");
    /* font file */
    CHECK(UP_SetFontFile((STRPTR) "FONTS:Test-I685h.ttf") == 0, "font add refused");
    const char* const* fl = mui_font_files();
    int found = 0;
    for (int i = 0; fl[i] != NULL; i++) {
        found += strcmp(fl[i], "FONTS:Test-I685h.ttf") == 0;
    }
    CHECK(found == 1, "font file not in the list");
    /* theme load: a missing file returns the store's -1 and leaves the tokens */
    uint32_t before[SEL_GUI_TOKEN_COUNT];
    memcpy(before, UP_ThemeTokens(), sizeof before);
    CHECK(UP_ThemeLoad((STRPTR) "/nonexistent/i685h/theme") == -1, "missing theme not -1");
    CHECK(memcmp(before, UP_ThemeTokens(), sizeof before) == 0, "tokens changed by a failed load");
    CHECK(mui_theme_requests() == 0, "failed load counted as request");
    printf("mcc-support-api OK\n");
    return 0;
}
