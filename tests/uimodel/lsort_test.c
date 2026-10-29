/* I611: the Your Library sort comparators and the text filter (src/uimodel/browse.c). */
#include <selecta/library_item.h>
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
    /* I678c: the kind-aware order (design section 5). Recents: the newer of last play and open, larger first. */
    {
        sel_uim_lsort_key played = {SEL_LIB_ITEM_ALBUM, "A", "x", 5, 300, 0}, opened = {SEL_LIB_ITEM_ALBUM, "B", "x", 5, 100, 400},
                          never = {SEL_LIB_ITEM_ALBUM, "C", "x", 5, 0, 0};
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_RECENTS, &opened, &played) < 0); /* opened 400 beats played 300 */
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_RECENTS, &played, &never) < 0);
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_RECENTS, &never, &never) == 0);
    }
    /* "The" is ignored for artists only (D4). */
    {
        sel_uim_lsort_key the_art = {SEL_LIB_ITEM_ARTIST, "The Beatles", "The Beatles", 0, 0, 0},
                          bob = {SEL_LIB_ITEM_ARTIST, "Bob", "Bob", 0, 0, 0}, ace = {SEL_LIB_ITEM_ARTIST, "Ace", "Ace", 0, 0, 0},
                          the_alb = {SEL_LIB_ITEM_ALBUM, "The Wall", "Pink", 0, 0, 0}, wall = {SEL_LIB_ITEM_ALBUM, "Wall", "Pink", 0, 0, 0},
                          bare = {SEL_LIB_ITEM_ARTIST, "The", "The", 0, 0, 0}, ten = {SEL_LIB_ITEM_ARTIST, "Theo", "Theo", 0, 0, 0};
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_ALPHA, &the_art, &bob) < 0 && sel_uim_lsort_item_cmp(SEL_UIM_LSORT_ALPHA, &the_art, &ace) > 0);
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_ALPHA, &the_alb, &wall) < 0); /* an album keeps its The: T before W */
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_ALPHA, &bare, &ten) < 0 && sel_uim_lsort_item_cmp(SEL_UIM_LSORT_ALPHA, &ten, &bare) > 0); /* "The" alone and "Theo" are not stripped */
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_CREATOR, &the_art, &bob) < 0); /* artists are their own creator */
    }
    /* Creator: albums by artist then name, ties by name. */
    {
        sel_uim_lsort_key a1 = {SEL_LIB_ITEM_ALBUM, "Zed", "Ann", 0, 0, 0}, a2 = {SEL_LIB_ITEM_ALBUM, "Abe", "Bob", 0, 0, 0},
                          a3 = {SEL_LIB_ITEM_ALBUM, "Abe", "Ann", 0, 0, 0};
        CHECK(sel_uim_lsort_item_cmp(SEL_UIM_LSORT_CREATOR, &a1, &a2) < 0 && sel_uim_lsort_item_cmp(SEL_UIM_LSORT_CREATOR, &a3, &a1) < 0);
    }
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
