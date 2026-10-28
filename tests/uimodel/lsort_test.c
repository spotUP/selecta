/* I611: the Your Library sort comparators and the text filter (src/uimodel/browse.c). */
#include <selecta/uimodel/browse.h>
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
    /* Recents keeps the engine order. */
    CHECK(sel_uim_lsort_cmp(SEL_UIM_LSORT_RECENTS, "b", 1, "You", "a", 2, "You") == 0);
    /* Recently added: the larger creation order first. */
    CHECK(sel_uim_lsort_cmp(SEL_UIM_LSORT_ADDED, "a", 1, "You", "b", 2, "You") > 0);
    CHECK(sel_uim_lsort_cmp(SEL_UIM_LSORT_ADDED, "a", 3, "You", "b", 2, "You") < 0);
    /* Alphabetical ignores case. */
    CHECK(sel_uim_lsort_cmp(SEL_UIM_LSORT_ALPHA, "beta", 1, "You", "Alpha", 2, "You") > 0);
    CHECK(sel_uim_lsort_cmp(SEL_UIM_LSORT_ALPHA, "alpha", 1, "You", "ALPHA", 2, "You") == 0);
    /* Creator first, then name. */
    CHECK(sel_uim_lsort_cmp(SEL_UIM_LSORT_CREATOR, "z", 1, "Ann", "a", 2, "Bob") < 0);
    CHECK(sel_uim_lsort_cmp(SEL_UIM_LSORT_CREATOR, "z", 1, "Ann", "a", 2, "Ann") > 0);
    /* Labels. */
    CHECK(sel_uim_lsort_label(SEL_UIM_LSORT_ADDED) != NULL && sel_uim_lsort_label(SEL_UIM_LSORTS) == NULL);
    CHECK(sel_uim_lview_label(SEL_UIM_LVIEW_LARGE) != NULL && sel_uim_lview_label(SEL_UIM_LVIEWS) == NULL);
    /* Filter: substring, case-insensitive, empty matches all. */
    CHECK(sel_uim_lfilter_match("Alpha Mix", "pha m") == 1);
    CHECK(sel_uim_lfilter_match("Alpha", "") == 1);
    CHECK(sel_uim_lfilter_match("Alpha", "beta") == 0);
    CHECK(sel_uim_lfilter_match("Al", "alp") == 0);
    if (g_fail == 0) printf("uim_lsort_ok\n");
    return g_fail ? 1 : 0;
}
