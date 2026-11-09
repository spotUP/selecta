/* I1222e: the sidebar stack (src/uimodel/sidestack.c): item order and heights for the text spec and the desktop spec, the icon rail gives
 * doors only, the top bar removes Home and Search, the brand drops under brand_min_w and when the list would get fewer than 3 rows, the
 * list takes the rest. */
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/sidestack.h>
#include <stdio.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static sel_uim_side_spec text_spec(void) {
    sel_uim_side_spec s = {1, 1, 1, 1, 1, 1, 12, 0, 1, 1};
    return s;
}

static sel_uim_side_spec desk_spec(void) {
    sel_uim_side_spec s = {40, 36, 36, 30, 28, 32, 120, 0, 1, 2};
    return s;
}

static uint32_t count_kind(const sel_uim_side_item* it, uint32_t n, sel_uim_side_kind k) {
    uint32_t i, c = 0;
    for (i = 0; i < n; i++) {
        c += it[i].kind == k;
    }
    return c;
}

int main(void) {
    sel_uim_side_item it[32];
    uint32_t n, i, doors_with_bar = 0, doors_without = 0;
    sel_uim_side_spec s = text_spec();

    for (i = 0; i < SEL_UIM_DOOR_COUNT; i++) {
        doors_with_bar += (uint32_t)sel_uim_door_shown_in(i, 1, 1);
        doors_without += (uint32_t)sel_uim_door_shown_in(i, 0, 1);
    }
    /* the top bar owns pages, so it removes doors */
    CHECK(doors_with_bar < doors_without);

    /* text spec, 25 cells wide, 30 high: brand, doors, header, chips, tools, list; every y follows the one before */
    n = sel_uim_side_stack(&s, 25, 30, it, 32u);
    CHECK(n == 1u + doors_with_bar + 1u + 1u + 1u + 1u);
    CHECK(it[0].kind == SEL_UIM_SIDE_BRAND && it[0].y == 0 && it[0].h == 1);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_DOOR) == doors_with_bar);
    CHECK(it[1 + doors_with_bar].kind == SEL_UIM_SIDE_LIBHDR);
    CHECK(it[n - 3u].kind == SEL_UIM_SIDE_CHIPS && it[n - 2u].kind == SEL_UIM_SIDE_TOOLS && it[n - 1u].kind == SEL_UIM_SIDE_LIST);
    for (i = 1; i < n; i++) {
        CHECK(it[i].y == it[i - 1u].y + it[i - 1u].h);
    }
    /* LIST takes the rest */
    CHECK(it[n - 1u].y + it[n - 1u].h == 30);
    /* the doors keep the sidebar order of the table */
    for (i = 2; i < 1u + doors_with_bar; i++) {
        CHECK(it[i].door > it[i - 1u].door);
    }

    /* the brand drops under brand_min_w */
    n = sel_uim_side_stack(&s, 11, 30, it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_BRAND) == 0u && it[0].kind == SEL_UIM_SIDE_DOOR && it[0].y == 0);

    /* the brand drops when the list would get fewer than 3 rows: fixed rows are doors + 3, list needs 3, brand 1 */
    n = sel_uim_side_stack(&s, 25, (int32_t)(doors_with_bar + 3u + 3u + 1u), it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_BRAND) == 1u && it[n - 1u].h == 3);
    n = sel_uim_side_stack(&s, 25, (int32_t)(doors_with_bar + 3u + 3u), it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_BRAND) == 0u && it[n - 1u].h == 3);

    /* no room at all: the list is empty, never negative */
    n = sel_uim_side_stack(&s, 25, 2, it, 32u);
    CHECK(it[n - 1u].kind == SEL_UIM_SIDE_LIST && it[n - 1u].h == 0);

    /* the icon rail: doors only (the rail shows every door the top bar leaves, header or not) */
    s.rail = 1;
    n = sel_uim_side_stack(&s, 8, 30, it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_DOOR) == n - count_kind(it, n, SEL_UIM_SIDE_BRAND));
    CHECK(count_kind(it, n, SEL_UIM_SIDE_LIBHDR) == 0u && count_kind(it, n, SEL_UIM_SIDE_LIST) == 0u);

    /* without a top bar every door is listed (with the header owning Library on the labelled sidebar) */
    s = text_spec();
    s.top_bar = 0;
    n = sel_uim_side_stack(&s, 25, 40, it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_DOOR) == doors_without);

    /* chips: none when chip_rows is 0, rows * row height otherwise (desktop spec) */
    s = desk_spec();
    n = sel_uim_side_stack(&s, 240, 800, it, 32u);
    CHECK(it[0].kind == SEL_UIM_SIDE_BRAND && it[0].h == 40);
    for (i = 0; i < n; i++) {
        if (it[i].kind == SEL_UIM_SIDE_CHIPS) {
            CHECK(it[i].h == 60);
        }
        if (it[i].kind == SEL_UIM_SIDE_DOOR) {
            CHECK(it[i].h == 36);
        }
    }
    s.chip_rows = 0;
    n = sel_uim_side_stack(&s, 240, 800, it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_CHIPS) == 0u);
    /* desktop short window: the brand drops */
    n = sel_uim_side_stack(&s, 240, 300, it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_BRAND) == 0u);
    /* desktop narrow rail-width below brand_min_w drops it too */
    n = sel_uim_side_stack(&s, 100, 800, it, 32u);
    CHECK(count_kind(it, n, SEL_UIM_SIDE_BRAND) == 0u);

    /* max caps the items written and returned: the buffer is never overrun */
    s = text_spec();
    n = sel_uim_side_stack(&s, 25, 30, it, 3u);
    CHECK(n == 3u);

    printf(g_fail == 0 ? "uim_sidestack_ok\n" : "uim_sidestack_fail %d\n", g_fail);
    return g_fail != 0;
}
