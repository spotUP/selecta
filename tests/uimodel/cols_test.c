/* I1079a: the shared column splitter (sel_uim_cols_split). */
#include <selecta/uimodel/cols.h>

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

static int32_t end_of(const int32_t* x, const int32_t* w, uint32_t i) { return x[i] + w[i]; }

static void test_weights(void) {
    int32_t x[8], w[8];
    sel_uim_col c[3] = {{45, 0, 0, 0}, {28, 0, 0, 0}, {27, 0, 0, 0}};
    uint32_t np = sel_uim_cols_split(703, 8, c, 3, x, w);
    CHECK(np == 3 && end_of(x, w, 2) == 703, "weights end at the edge: %d", end_of(x, w, 2));
    CHECK(x[1] == w[0] + 8 && x[2] == x[1] + w[1] + 8, "gaps between columns");
    CHECK(w[0] > w[1] && w[1] > w[2], "weights order %d %d %d", w[0], w[1], w[2]);
}

static void test_absent(void) {
    int32_t x[8], w[8], x3[3], w3[3];
    sel_uim_col c[3] = {{45, 0, 0, 0}, {0, 0, 0, 0}, {27, 0, 0, 0}};
    sel_uim_col all[3] = {{45, 0, 0, 0}, {28, 0, 0, 0}, {27, 0, 0, 0}};
    uint32_t np = sel_uim_cols_split(600, 10, c, 3, x, w);
    CHECK(np == 2 && w[1] == 0 && x[2] == w[0] + 10 && end_of(x, w, 2) == 600, "absent: np %u w0 %d x2 %d", np, w[0], x[2]);
    sel_uim_cols_split(600, 10, all, 3, x3, w3);
    CHECK(w[0] > w3[0], "absent column width goes to the others (%d > %d)", w[0], w3[0]);
}

static void test_fixed(void) {
    int32_t x[8], w[8];
    sel_uim_col c[3] = {{3, 0, 0, 0}, {0, 56, 0, 0}, {0, 48, 0, 0}};
    sel_uim_cols_split(500, 4, c, 3, x, w);
    CHECK(w[1] == 56 && w[2] == 48 && w[0] == 500 - 56 - 48 - 8 && end_of(x, w, 2) == 500, "fixed: %d %d %d", w[0], w[1], w[2]);
}

static void test_min_max(void) {
    int32_t x[8], w[8];
    sel_uim_col c[2] = {{1, 0, 0, 0}, {9, 0, 0, 0}};
    sel_uim_col m[2] = {{1, 0, 100, 0}, {9, 0, 0, 0}};
    sel_uim_col mx[2] = {{1, 0, 0, 50}, {1, 0, 0, 0}};
    sel_uim_cols_split(400, 0, c, 2, x, w);
    CHECK(w[0] == 40, "unpinned share %d", w[0]);
    sel_uim_cols_split(400, 0, m, 2, x, w);
    CHECK(w[0] == 100 && w[1] == 300, "min pinned: %d %d", w[0], w[1]);
    sel_uim_cols_split(400, 0, mx, 2, x, w);
    CHECK(w[0] == 50 && w[1] == 350, "max pinned: %d %d", w[0], w[1]);
}

static void test_narrow(void) {
    int32_t x[8], w[8];
    uint32_t i;
    sel_uim_col c[2] = {{1, 0, 100, 0}, {1, 0, 300, 0}};
    sel_uim_col f[3] = {{1, 0, 40, 0}, {0, 100, 0, 0}, {0, 100, 0, 0}};
    sel_uim_cols_split(200, 0, c, 2, x, w);
    CHECK(w[0] == 50 && w[1] == 150, "proportional minimums %d %d", w[0], w[1]);
    sel_uim_cols_split(20, 6, f, 3, x, w);
    CHECK(w[0] == 0 && w[1] == 100 && w[2] == 100, "narrow row: flex 0 (%d)", w[0]);
    for (i = 0; i < 3; i++) {
        CHECK(w[i] >= 0, "negative width");
    }
}

static void test_remainder(void) {
    int32_t x[8], w[8];
    sel_uim_col c[3] = {{1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}};
    sel_uim_cols_split(100, 0, c, 3, x, w);
    CHECK(w[0] + w[1] + w[2] == 100 && w[0] == 33 && w[1] == 33 && w[2] == 34, "remainder: %d %d %d", w[0], w[1], w[2]);
}

int main(void) {
    uint32_t calls0 = sel_uim_cols_calls();
    test_weights();
    test_absent();
    test_fixed();
    test_min_max();
    test_narrow();
    test_remainder();
    CHECK(sel_uim_cols_calls() >= calls0 + 9, "sentinel did not move: %u", sel_uim_cols_calls() - calls0);
    if (g_fail == 0) {
        printf("uim_cols_ok\n");
    }
    return g_fail != 0;
}
