/* I831: the left button of the label control "< label >" (src/uimodel/libfilter.c): Previous Filter with All in force, Clear Filter (the x)
 * while any other filter is in force; the right button always steps on, clamped. */
#include <selecta/uimodel/libfilter.h>
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
    uint32_t f;
    CHECK(!sel_uim_lfilter_left_clears(0u));
    CHECK(strcmp(sel_uim_lfilter_left_name(0u), "Previous Filter") == 0);
    CHECK(sel_uim_lfilter_single_step(0u, -1) == 0u);
    CHECK(sel_uim_lfilter_single_step(0u, 1) == 1u);
    for (f = 1u; f < SEL_UIM_LFILTERS; f++) {
        CHECK(sel_uim_lfilter_left_clears(f));
        CHECK(strcmp(sel_uim_lfilter_left_name(f), "Clear Filter") == 0);
        CHECK(sel_uim_lfilter_single_step(f, -1) == 0u); /* the x returns to All, not one step back */
        CHECK(sel_uim_lfilter_single_step(f, 1) == (f + 1u < SEL_UIM_LFILTERS ? f + 1u : f));
    }
    printf(g_fail == 0 ? "uim_libfilter_left_ok\n" : "uim_libfilter_left_fail %d\n", g_fail);
    return g_fail != 0;
}
