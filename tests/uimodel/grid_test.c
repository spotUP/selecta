/* I1473g1: shared cover-grid geometry (sel_uim_grid_cols / first_row / hit). */
#include <selecta/uimodel/grid.h>

#include <stdio.h>

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

/* 3 cols, tile 100x120, gap 10, origin (20,30), 2 rows visible, first row 1, window starts at tile 3 with 5 rows */
#define HIT(x, y) sel_uim_grid_hit((x), (y), 20, 30, 100, 120, 10, 3, 1, 2, 3, 5)

int main(void) {
    /* tile 100 wide, gap 10: n tiles need 110n - 10 */
    CHECK(sel_uim_grid_cols(100, 100, 10) == 1u, "exact one tile");
    CHECK(sel_uim_grid_cols(209, 100, 10) == 1u, "just short of two");
    CHECK(sel_uim_grid_cols(210, 100, 10) == 2u, "exactly two");
    CHECK(sel_uim_grid_cols(5, 100, 10) == 1u, "narrower than a tile is one column");
    CHECK(sel_uim_grid_cols(-50, 100, 10) == 1u, "negative width is one column");
    CHECK(sel_uim_grid_first_row(0, 4, 3) == 0u, "cursor at top");
    CHECK(sel_uim_grid_first_row(11, 4, 3) == 0u, "row 2 of 3 visible");
    CHECK(sel_uim_grid_first_row(12, 4, 3) == 1u, "row 3 scrolls one");
    CHECK(sel_uim_grid_first_row(40, 4, 0) == 0u, "no visible rows");
    CHECK(sel_uim_grid_first_row(9, 0, 3) == 0u, "zero cols is safe");
    CHECK(HIT(25, 35) == 0, "first drawn tile = abs 3 = window 0");
    CHECK(HIT(20 + 110 * 2 + 5, 35) == 2, "third column");
    CHECK(HIT(20 + 105, 35) == -1, "gap between columns");
    CHECK(HIT(25, 30 + 125) == -1, "gap between rows");
    CHECK(HIT(25, 30 + 130 + 5) == 3, "second drawn row = abs 6 = window 3");
    CHECK(HIT(20 + 110 * 2 + 5, 30 + 130 + 5) == -1, "window index 5 is outside row_n 5");
    CHECK(HIT(5, 35) == -1, "left margin");
    CHECK(HIT(25, 30 + 260 + 5) == -1, "below the visible rows");
    if (g_fail == 0) {
        printf("uim_grid_ok\n");
    }
    return g_fail != 0;
}
