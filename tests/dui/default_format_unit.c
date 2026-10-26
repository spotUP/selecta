/* I522: `--backend aga` without `--format` must open, so the default format of the aga backend is index8 (the AGA screen
 * shows an 8-bitplane CLUT and desk_aga_open refuses rgba32). The desktop UI feeds this choice to sel_uim_budget_for. */
#include <selecta/uimodel.h>

#include <stdio.h>

static int fails;

#define CHECK(c) \
    do { \
        if (!(c)) { \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            fails++; \
        } \
    } while (0)

static uint32_t format_of(const char* backend, const char* explicit_format) {
    sel_uim_budget b;
    const char* f = explicit_format != NULL ? explicit_format : sel_uim_default_format(backend);
    CHECK(sel_uim_budget_for(&b, f));
    return b.format;
}

int main(void) {
    CHECK(format_of("aga", NULL) == SEL_UIM_FMT_INDEX8);
    CHECK(format_of("aga", "rgba32") == SEL_UIM_FMT_RGBA32); /* explicit rgba32 stays rgba32 (the backend then refuses it) */
    CHECK(format_of("rtg", NULL) == SEL_UIM_FMT_RGBA32);
    CHECK(format_of("sdl", NULL) == SEL_UIM_FMT_RGBA32);
    CHECK(format_of("mem", NULL) == SEL_UIM_FMT_RGBA32);
    CHECK(format_of(NULL, NULL) == SEL_UIM_FMT_RGBA32);
    CHECK(format_of("mem", "index8") == SEL_UIM_FMT_INDEX8);
    printf(fails == 0 ? "default_format_ok\n" : "default_format_failed\n");
    return fails == 0 ? 0 : 1;
}
