/* I994d1: sel_coll_order_build (pure): every sort key in both directions, the three-field filter, tie order, the title fallback. */
#include <selecta/engine.h>
#include <stdio.h>
#include <string.h>

#include "collorder.h"

#define N 5u

/* Stored order 0..4. Row 3 has no catalogue title: it shows (and sorts as) its file name "alpha.mod". Rows 1 and 4 tie on
 * every sort key except title. */
static const sel_coll_rec REC[N] = {
    {"local:/m/zed.mod", "Zed", "Mike", "Blue", 300, 90000},
    {"local:/m/b.mod", "bravo", "adam", "Red", 100, 60000},
    {"local:/m/c.mod", "Charlie", "Zoe", "green", 500, 30000},
    {"local:/m/alpha.mod", "", "Mike", "Blue", 200, 60000},
    {"local:/m/e.mod", "Echo", "adam", "Red", 100, 60000},
};

static int fails;

static void expect(const char* what, uint32_t sort, uint32_t desc, const char* filter, const uint32_t* want, uint32_t want_n) {
    uint32_t out[N], got, i;
    got = sel_coll_order_build(REC, N, sort, desc, filter, out);
    if (got != want_n) {
        printf("FAIL %s: shown %u want %u\n", what, got, want_n);
        fails++;
        return;
    }
    for (i = 0; i < want_n; i++) {
        if (out[i] != want[i]) {
            printf("FAIL %s: pos %u row %u want %u\n", what, i, out[i], want[i]);
            fails++;
            return;
        }
    }
}

int main(void) {
    /* Title (case-insensitive, row 3 = "alpha.mod"): alpha(3) bravo(1) Charlie(2) Echo(4) Zed(0) */
    static const uint32_t t_asc[] = {3, 1, 2, 4, 0}, t_desc[] = {0, 4, 2, 1, 3};
    /* Artist: adam(1,4 tie) Mike(0,3 tie) Zoe(2) */
    static const uint32_t a_asc[] = {1, 4, 0, 3, 2}, a_desc[] = {2, 0, 3, 1, 4};
    /* Album: Blue(0,3) green(2) Red(1,4) */
    static const uint32_t l_asc[] = {0, 3, 2, 1, 4}, l_desc[] = {1, 4, 2, 0, 3};
    /* Date added: 100(1,4) 200(3) 300(0) 500(2) */
    static const uint32_t d_asc[] = {1, 4, 3, 0, 2}, d_desc[] = {2, 0, 3, 1, 4};
    /* Duration: 30000(2) 60000(1,3,4) 90000(0) */
    static const uint32_t u_asc[] = {2, 1, 3, 4, 0}, u_desc[] = {0, 1, 3, 4, 2};
    static const uint32_t c_asc[] = {0, 1, 2, 3, 4}, c_desc[] = {4, 3, 2, 1, 0};
    /* Filter "bl": album Blue (rows 0,3) only; title-only and artist-only matches are the cases below */
    static const uint32_t f_bl[] = {0, 3};
    /* Filter "ADAM" by artist, Duration descending: rows 1,4 tie -> stored order */
    static const uint32_t f_adam[] = {1, 4};
    /* Filter "alpha" matches only the file-name fallback title of row 3 */
    static const uint32_t f_alpha[] = {3};
    /* Filter "red" matches album of rows 1,4, sorted by title descending: Echo(4) bravo(1) */
    static const uint32_t f_red[] = {4, 1};

    expect("custom asc", SEL_COLL_SORT_CUSTOM, 0, "", c_asc, N);
    expect("custom desc", SEL_COLL_SORT_CUSTOM, 1, "", c_desc, N);
    expect("title asc", SEL_COLL_SORT_TITLE, 0, "", t_asc, N);
    expect("title desc", SEL_COLL_SORT_TITLE, 1, "", t_desc, N);
    expect("artist asc", SEL_COLL_SORT_ARTIST, 0, "", a_asc, N);
    expect("artist desc", SEL_COLL_SORT_ARTIST, 1, "", a_desc, N);
    expect("album asc", SEL_COLL_SORT_ALBUM, 0, "", l_asc, N);
    expect("album desc", SEL_COLL_SORT_ALBUM, 1, "", l_desc, N);
    expect("added asc", SEL_COLL_SORT_ADDED, 0, "", d_asc, N);
    expect("added desc", SEL_COLL_SORT_ADDED, 1, "", d_desc, N);
    expect("duration asc", SEL_COLL_SORT_DURATION, 0, "", u_asc, N);
    expect("duration desc", SEL_COLL_SORT_DURATION, 1, "", u_desc, N);
    expect("filter title+album", SEL_COLL_SORT_CUSTOM, 0, "bl", f_bl, 2);
    expect("filter artist", SEL_COLL_SORT_DURATION, 1, "ADAM", f_adam, 2);
    expect("filter file-name title", SEL_COLL_SORT_CUSTOM, 0, "alpha", f_alpha, 1);
    expect("filter album + title desc", SEL_COLL_SORT_TITLE, 1, "red", f_red, 2);
    expect("filter none match", SEL_COLL_SORT_TITLE, 0, "qqq", NULL, 0);

    if (strcmp(sel_coll_title(&REC[3]), "alpha.mod") != 0 || strcmp(sel_coll_title(&REC[0]), "Zed") != 0) {
        printf("FAIL title fallback\n");
        fails++;
    }
    if (fails != 0) return 1;
    printf("collection_order_ok\n");
    return 0;
}
