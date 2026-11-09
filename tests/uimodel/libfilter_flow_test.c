/* I1222d: the shared chip wrap (src/uimodel/libfilter.c, sel_uim_chips_flow). One row when all fit, wrap at exact fit plus one, a chip
 * wider than the band, no chips. */
#include <selecta/uimodel/libfilter.h>
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
    int32_t w[4] = {10, 20, 30, 40}, x[4], r[4], px, pr;
    uint32_t k;
    /* all fit: 10+5+20+5+30+5+40 = 115 */
    CHECK(sel_uim_chips_flow(w, 4u, 5, 115, x, r) == 1);
    CHECK(x[0] == 0 && x[1] == 15 && x[2] == 40 && x[3] == 75);
    for (k = 0; k < 4u; k++) {
        CHECK(r[k] == 0);
    }
    /* one short: the last chip wraps to row 1 at x 0 */
    CHECK(sel_uim_chips_flow(w, 4u, 5, 114, x, r) == 2);
    CHECK(r[2] == 0 && r[3] == 1 && x[3] == 0);
    /* a chip wider than the band sits alone on its row, never skipped */
    CHECK(sel_uim_chips_flow(w, 4u, 5, 25, x, r) == 4);
    CHECK(r[0] == 0 && x[0] == 0 && r[1] == 1 && x[1] == 0 && r[2] == 2 && x[2] == 0 && r[3] == 3 && x[3] == 0 + 0);
    /* a wide chip first: it stays on row 0 */
    CHECK(sel_uim_chips_flow(w + 3, 1u, 5, 10, x, r) == 1 && x[0] == 0 && r[0] == 0);
    /* no chips */
    CHECK(sel_uim_chips_flow(w, 0u, 5, 100, x, r) == 0);
    /* place agrees with flow, and k past the end gives the spot after the last chip */
    sel_uim_chips_place(w, 4u, 5, 114, 3u, &px, &pr);
    CHECK(px == 0 && pr == 1);
    sel_uim_chips_place(w, 4u, 5, 114, 4u, &px, &pr);
    CHECK(px == 45 && pr == 1);
    printf(g_fail == 0 ? "uim_libfilter_flow_ok\n" : "uim_libfilter_flow_fail %d\n", g_fail);
    return g_fail != 0;
}
