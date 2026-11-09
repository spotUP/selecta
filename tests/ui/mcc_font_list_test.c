/* I685e2 part 2: the package font search list. Modes:
 * order:  setter paths first (call order), theme key font.file second, FONTS:Inter-Regular.ttf last, duplicates skipped.
 * source: no "Selecta:" and no "RAM:" string literal anywhere under the package src/. */
#include "map/mui_theme.h"

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

static int count(const char* const* l) {
    int n = 0;
    while (l[n] != NULL) n++;
    return n;
}

static int test_order(const char* tmp) {
    txt_theme th;
    const char* const* l;
    FILE* f = fopen(tmp, "wb");
    CHECK(f != NULL, "cannot write %s", tmp);
    fputs("font.file = DH0:Fonts/Theme.ttf  # comment\n", f);
    fclose(f);

    l = mui_font_files();
    CHECK(count(l) == 1 && strcmp(l[0], MUI_FONT_DEFAULT) == 0, "empty state is not just the default (%d)", count(l));

    CHECK(mui_font_file_add("A:one.ttf") == 0 && mui_font_file_add("A:two.ttf") == 0 && mui_font_file_add("A:one.ttf") == 0, "add");
    CHECK(mui_font_file_add("") == -1 && mui_font_file_add(NULL) == -1, "empty add accepted");
    mui_theme_resolve(&th, NULL, tmp);
    CHECK(strcmp(th.font_file, "DH0:Fonts/Theme.ttf") == 0, "theme key parsed as '%s'", th.font_file);
    l = mui_font_files();
    CHECK(count(l) == 4, "expected 4 entries, got %d", count(l));
    CHECK(strcmp(l[0], "A:one.ttf") == 0 && strcmp(l[1], "A:two.ttf") == 0, "setter paths not first, in call order");
    CHECK(strcmp(l[2], "DH0:Fonts/Theme.ttf") == 0, "theme key not third: %s", l[2]);
    CHECK(strcmp(l[3], "FONTS:Inter-Regular.ttf") == 0, "default not last: %s", l[3]);

    /* a theme without the key drops the theme entry; a theme naming the default adds no duplicate */
    mui_theme_resolve(&th, NULL, NULL);
    l = mui_font_files();
    CHECK(count(l) == 3 && strcmp(l[2], MUI_FONT_DEFAULT) == 0, "theme without key keeps an entry");
    f = fopen(tmp, "wb");
    CHECK(f != NULL, "rewrite");
    fputs("font.file = FONTS:Inter-Regular.ttf\n", f);
    fclose(f);
    mui_theme_resolve(&th, NULL, tmp);
    CHECK(count(mui_font_files()) == 3, "duplicate default listed twice");
    return 0;
}

static int test_source(void) {
    static const char* const files[] = {"/src/mui_aa_draw.c", "/src/map/mui_theme.c", "/src/mui_aatip.c", "/src/class/mui_aafield_class.c"};
    static char buf[300000];
    size_t i;
    for (i = 0; i < sizeof files / sizeof files[0]; i++) {
        char path[1024];
        size_t n;
        FILE* f;
        snprintf(path, sizeof path, "%s%s", MCC_DIR, files[i]);
        f = fopen(path, "rb");
        CHECK(f != NULL, "cannot read %s", path);
        n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = '\0';
        CHECK(strstr(buf, "\"Selecta:") == NULL, "%s holds a \"Selecta: literal", files[i]);
        CHECK(strstr(buf, "\"RAM:") == NULL, "%s holds a \"RAM: literal", files[i]);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && !strcmp(argv[1], "order")) return test_order(argv[2]);
    if (argc >= 2 && !strcmp(argv[1], "source")) return test_source();
    fprintf(stderr, "usage: mcc_font_list_test order <tmpfile> | source\n");
    return 2;
}
