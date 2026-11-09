/* I685j: the UP-MCC.mcp prefs page (design 2026-11-06-mui-mcc-package.md section 5), host part. One entry point per area:
 *   writer   txt_theme_write -> txt_theme_parse gives the same tokens
 *   mapping  tokens -> gadget values -> tokens is lossless for accent, surface, text, background, radius (src/map/up_prefs_map.c,
 *            the pure half of plugins/shared/mcc/prefs/up_prefs.c)
 *   themes   dist/themes/Dark.theme equals the built-in defaults; Light.theme parses and differs
 *   complete   every file of dist/themes named *.theme holds exactly the keys of SEL_GUI_TOKENS (none missing, none unknown, each once, in range)
 *   cmake    the CMake build list names UP-MCC.mcp (target up_mcc_prefs) */
#include "map/up_prefs_map.h"

#include <selecta/uimodel/theme.h>
#include <selecta/uimodel/tokens.h>

#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
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

static char* slurp(const char* path) {
    char* buf = malloc(65536);
    FILE* f = fopen(path, "rb");
    if (buf == NULL || f == NULL) {
        free(buf);
        if (f != NULL) fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, 65535, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static int same_tokens(const txt_theme* a, const txt_theme* b) {
    return memcmp(a->token, b->token, sizeof a->token) == 0;
}

static int t_writer(void) {
    txt_theme src, back;
    char text[4096];
    int bad = 0;
    txt_theme_default(&src);
    for (int i = 0; i < SEL_GUI_TOKEN_COUNT; i++) {
        uint32_t lo = sel_gui_token_min((sel_gui_token)i), hi = sel_gui_token_max((sel_gui_token)i);
        src.token[i] = hi == 0xFFFFFFu ? 0x0A0B0Cu + (uint32_t)i * 0x010203u : lo + (uint32_t)(i % 3);
    }
    int n = txt_theme_write(&src, text, sizeof text);
    CHECK(n > 0 && (size_t)n == strlen(text), "write length %d", n);
    CHECK(strstr(text, "gui.accent = 0x") != NULL, "no accent line");
    txt_theme_default(&back);
    CHECK(txt_theme_parse(&back, text, &bad) > 0, "parse failed at line %d", bad);
    CHECK(same_tokens(&src, &back), "tokens differ after the round trip");
    char tiny[16];
    CHECK(txt_theme_write(&src, tiny, sizeof tiny) == -1, "small buffer not refused");
    return 0;
}

static int t_mapping(void) {
    uint32_t tok[SEL_GUI_TOKEN_COUNT], out[SEL_GUI_TOKEN_COUNT];
    up_prefs_gadgets g;
    txt_theme th;
    txt_theme_default(&th);
    memcpy(tok, th.token, sizeof tok);
    tok[SEL_GUI_TOKEN_ACCENT] = 0x2563EBu;
    tok[SEL_GUI_TOKEN_SURFACE] = 0xFFFFFFu;
    tok[SEL_GUI_TOKEN_TEXT] = 0x1A1A1Eu;
    tok[SEL_GUI_TOKEN_BACKGROUND] = 0x000001u;
    tok[SEL_GUI_TOKEN_RADIUS] = 17;
    up_prefs_tokens_to_gadgets(tok, &g);
    CHECK(strcmp(g.accent, "2563EB") == 0 && g.radius == 17, "gadget values %s %d", g.accent, g.radius);
    memcpy(out, th.token, sizeof out);
    CHECK(up_prefs_gadgets_to_tokens(&g, out) == 0, "gadgets refused");
    CHECK(memcmp(tok, out, sizeof tok) == 0, "mapping is not lossless");
    /* lower case and a 0x prefix are accepted; garbage is refused and leaves the tokens untouched */
    strcpy(g.accent, "0xabcdef");
    CHECK(up_prefs_gadgets_to_tokens(&g, out) == 0 && out[SEL_GUI_TOKEN_ACCENT] == 0xABCDEFu, "0x prefix");
    strcpy(g.accent, "12G456");
    uint32_t before = out[SEL_GUI_TOKEN_ACCENT];
    CHECK(up_prefs_gadgets_to_tokens(&g, out) == -1 && out[SEL_GUI_TOKEN_ACCENT] == before, "bad hex not refused");
    /* radius is clamped to the token range */
    up_prefs_tokens_to_gadgets(tok, &g);
    g.radius = 999;
    CHECK(up_prefs_gadgets_to_tokens(&g, out) == 0 && out[SEL_GUI_TOKEN_RADIUS] == sel_gui_token_max(SEL_GUI_TOKEN_RADIUS), "radius clamp");
    return 0;
}

static int t_themes(const char* dir) {
    char path[512];
    txt_theme def, dark, light;
    int bad = 0;
    txt_theme_default(&def);
    snprintf(path, sizeof path, "%s/Dark.theme", dir);
    char* text = slurp(path);
    CHECK(text != NULL, "cannot read %s", path);
    txt_theme_default(&dark);
    dark.token[SEL_GUI_TOKEN_ACCENT] ^= 1;
    CHECK(txt_theme_parse(&dark, text, &bad) > 0, "Dark.theme line %d", bad);
    free(text);
    CHECK(same_tokens(&def, &dark), "Dark.theme differs from the built-in defaults");
    snprintf(path, sizeof path, "%s/Light.theme", dir);
    text = slurp(path);
    CHECK(text != NULL, "cannot read %s", path);
    txt_theme_default(&light);
    CHECK(txt_theme_parse(&light, text, &bad) > 0, "Light.theme line %d", bad);
    free(text);
    CHECK(light.token[SEL_GUI_TOKEN_BACKGROUND] == 0xF4F4F6u && light.token[SEL_GUI_TOKEN_ACCENT] == 0x2563EBu, "Light.theme values");
    CHECK(light.token[SEL_GUI_TOKEN_SURFACE] == 0xFFFFFFu && light.token[SEL_GUI_TOKEN_ERROR] != def.token[SEL_GUI_TOKEN_ERROR], "Light.theme still keeps a dark default for the status colours");
    return 0;
}

/* I685o: one theme file names every SEL_GUI_TOKENS key exactly once and nothing else; the loader keeps the built-in dark value
 * for a key a file leaves out, so a missing key in Light.theme is a dark remnant on a light surface. */
static int theme_complete(const char* path, const char* text) {
    int seen[SEL_GUI_TOKEN_COUNT] = {0};
    int lineno = 0;
    for (const char* p = text; *p != '\0';) {
        const char* eol = strchr(p, '\n');
        size_t len = eol != NULL ? (size_t)(eol - p) : strlen(p);
        char line[256];
        lineno++;
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        p += len + (eol != NULL ? 1 : 0);
        if (line[0] == '#' || line[0] == '\0') continue;
        char* eq = strchr(line, '=');
        CHECK(eq != NULL, "%s:%d no '=' in \"%s\"", path, lineno, line);
        char* e = eq;
        while (e > line && e[-1] == ' ') e--;
        *e = '\0';
        int idx = -1;
        for (int i = 0; i < SEL_GUI_TOKEN_COUNT; i++)
            if (strcmp(line, sel_gui_token_name((sel_gui_token)i)) == 0) idx = i;
        CHECK(idx >= 0, "%s:%d unknown token key %s", path, lineno, line);
        CHECK(!seen[idx], "%s:%d token %s named twice", path, lineno, line);
        seen[idx] = 1;
        unsigned long v = strtoul(eq + 1, NULL, 0);
        CHECK(v >= sel_gui_token_min((sel_gui_token)idx) && v <= sel_gui_token_max((sel_gui_token)idx), "%s:%d %s out of range", path, lineno, line);
    }
    for (int i = 0; i < SEL_GUI_TOKEN_COUNT; i++)
        CHECK(seen[i], "%s misses token %s", path, sel_gui_token_name((sel_gui_token)i));
    return 0;
}

static int t_complete(const char* dir) {
    int files = 0;
    DIR* d = opendir(dir);
    CHECK(d != NULL, "cannot open %s", dir);
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n < 7 || strcmp(de->d_name + n - 6, ".theme") != 0) continue;
        char path[512];
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        char* text = slurp(path);
        CHECK(text != NULL, "cannot read %s", path);
        int rc = theme_complete(path, text);
        free(text);
        if (rc != 0) {
            closedir(d);
            return rc;
        }
        files++;
    }
    closedir(d);
    CHECK(files >= 2, "only %d theme files in %s", files, dir);
    printf("[OK] mcc-theme-complete: %d theme files each name all %d tokens\n", files, (int)SEL_GUI_TOKEN_COUNT);
    return 0;
}

static int t_cmake(const char* cmake_file, const char* name) {
    char* text = slurp(cmake_file);
    CHECK(text != NULL, "cannot read %s", cmake_file);
    CHECK(strstr(text, "up_mcc_prefs") != NULL, "no up_mcc_prefs target");
    CHECK(strstr(text, "up_mcc_prefs PROPERTIES OUTPUT_NAME \"${MCC_PREFS_NAME}\"") != NULL && strstr(text, "set(MCC_PREFS_NAME \"UP-MCC.mcp\"") != NULL, "OUTPUT_NAME of up_mcc_prefs is not the UP-MCC.mcp cache variable");
    free(text);
    CHECK(strcmp(name, "UP-MCC.mcp") == 0, "cache variable MCC_PREFS_NAME is %s", name);
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3 && strcmp(argv[1], "themes") == 0) return t_themes(argv[2]);
    if (argc >= 3 && strcmp(argv[1], "complete") == 0) return t_complete(argv[2]);
    if (argc >= 4 && strcmp(argv[1], "cmake") == 0) return t_cmake(argv[2], argv[3]);
    if (argc >= 2 && strcmp(argv[1], "writer") == 0) return t_writer();
    if (argc >= 2 && strcmp(argv[1], "mapping") == 0) return t_mapping();
    fprintf(stderr, "usage: mcc_prefs_test writer|mapping|themes <dir>|complete <dir>|cmake <CMakeLists> <name>\n");
    return 2;
}
