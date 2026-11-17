/* I1442b: the shared drill rules lifted into src/uimodel/drill.c (key round-trip, crumbs, engine-find, filterable, count). */
#include <selecta/engine.h>
#include <selecta/uimodel/drill.h>

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            g_fail++; \
            printf("FAIL line %d: ", __LINE__); \
            printf(__VA_ARGS__); \
            printf("\n"); \
        } \
    } while (0)

static void roundtrip(const char* src, const char* path, uint32_t skip, const char* find) {
    char key[192], s[64], p[160], f[64];
    uint32_t sk = 99u;
    sel_uim_drill_key(src, path, skip, find, key, sizeof key);
    sel_uim_drill_key_parse(key, s, sizeof s, p, sizeof p, &sk, f, sizeof f);
    CHECK(strcmp(s, src) == 0 && strcmp(p, path) == 0 && sk == skip && strcmp(f, find) == 0, "roundtrip [%s][%s] -> [%s][%s] %u [%s]", src, path, s, p, (unsigned)sk, f);
}

static void crumb(const char* path, uint32_t want_n, const char* last_label, const char* last_path) {
    char l[4][96], c[4][160];
    uint32_t n = sel_uim_drill_crumbs("Modland", path, l, c);
    CHECK(n == want_n && strcmp(l[0], "Modland") == 0, "crumbs [%s] n=%u", path, (unsigned)n);
    CHECK(n == want_n && strcmp(l[n - 1u], last_label) == 0 && strcmp(c[n - 1u], last_path) == 0, "crumbs [%s] last [%s] [%s]", path, l[n - 1u], c[n - 1u]);
}

int main(void) {
    char out[64];
    roundtrip("hvsc", "", 0u, "");
    roundtrip("hvsc", "formats", 0u, "");
    roundtrip("hvsc", "formats", 200u, "");
    roundtrip("hvsc", "format\x1f" "Protracker", 0u, "pro");
    roundtrip("hvsc", "format\x1f" "Protracker\x1f" "Artist 1", 400u, "x y");
    crumb("", 1u, "Modland", "");
    crumb("formats", 2u, "Formats", "formats");
    crumb("format\x1f" "Protracker", 3u, "Protracker", "format\x1f" "Protracker");
    crumb("format\x1f" "Protracker\x1f" "Artist 1", 4u, "Artist 1", "format\x1f" "Protracker\x1f" "Artist 1");
    crumb("platform\x1f" "amiga", 3u, "Amiga", "platform\x1f" "amiga");
    crumb("tags", 2u, "Tags", "tags");
    CHECK(sel_uim_drill_engine_find("formats") && sel_uim_drill_engine_find("artists") && sel_uim_drill_engine_find("format\x1f" "X"), "engine find yes");
    CHECK(!sel_uim_drill_engine_find("platforms") && !sel_uim_drill_engine_find("") && !sel_uim_drill_engine_find("format\x1f" "X\x1f" "Y") &&
              !sel_uim_drill_engine_find("formatsx") && !sel_uim_drill_engine_find("tags"), "engine find no");
    CHECK(sel_uim_drill_filterable(SEL_DRILL_PAGE_GROUPS, "formats", 200u, 200u, ""), "engine level any window");
    CHECK(!sel_uim_drill_filterable(SEL_DRILL_PAGE_GROUPS, "formats", 0u, 0u, ""), "engine level empty hidden");
    CHECK(sel_uim_drill_filterable(SEL_DRILL_PAGE_GROUPS, "formats", 0u, 0u, "zz"), "engine level kept while a find is active");
    CHECK(sel_uim_drill_filterable(SEL_DRILL_PAGE_GROUPS, "platforms", 20u, 0u, ""), "local level one window");
    CHECK(!sel_uim_drill_filterable(SEL_DRILL_PAGE_GROUPS, "platforms", 200u, 0u, ""), "local level full window hidden");
    CHECK(!sel_uim_drill_filterable(SEL_DRILL_PAGE_GROUPS, "platforms", 20u, 200u, ""), "local level skipped hidden");
    CHECK(!sel_uim_drill_filterable(SEL_DRILL_PAGE_ROOT, "", 4u, 0u, ""), "root has no field");
    sel_uim_drill_count(SEL_DRILL_PAGE_ROOT, "4", out, sizeof out);
    CHECK(strcmp(out, "4") == 0, "root count [%s]", out);
    sel_uim_drill_count(SEL_DRILL_PAGE_GROUPS, "1333", out, sizeof out);
    CHECK(strcmp(out, "1333 songs") == 0, "group count [%s]", out);
    sel_uim_drill_count(SEL_DRILL_PAGE_GROUPS, "", out, sizeof out);
    CHECK(out[0] == '\0', "unknown count");
    if (g_fail == 0) {
        printf("uim_drill_ok\n");
    }
    return g_fail != 0;
}
