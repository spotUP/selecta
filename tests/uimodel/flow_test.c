/* I1473g8b1: shared Cover Flow geometry (sel_uim_flow_*). */
#include <selecta/uimodel/flow.h>

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

int main(void) {
    int32_t sz, off, al, k;
    const int32_t sz_exp[4] = {200, 160, 120, 80}, off_exp[4] = {0, 50, 100, 150}, al_exp[4] = {0, 90, 150, 150};
    sel_uim_flow_cover c[SEL_UIM_FLOW_MAX];
    uint32_t n, i;

    for (k = 0; k < 4; k++) {
        sel_uim_flow_slot(200, k * 65536, &sz, &off, &al);
        CHECK(sz == sz_exp[k] && off == off_exp[k] && al == al_exp[k], "slot %d: %d %d %d", (int)k, (int)sz, (int)off, (int)al);
    }
    sel_uim_flow_slot(200, 3 * 65536 + 32768, &sz, &off, &al);
    CHECK(sz == 40 && off == 150 && al == 150, "slot 3.5 is half the slot-3 size: %d %d %d", (int)sz, (int)off, (int)al);
    sel_uim_flow_slot(200, 32768, &sz, &off, &al);
    CHECK(sz == 180 && off == 25 && al == 45, "slot 0.5: %d %d %d", (int)sz, (int)off, (int)al);

    CHECK(sel_uim_flow_stack_w(200) == 380, "stack 1.9 x big");
    CHECK(sel_uim_flow_big(380, 1000, 0, 200, 48) == 200, "exact width fit");
    CHECK(sel_uim_flow_big(380, 150, 50, 200, 48) == 100, "height limits");
    CHECK(sel_uim_flow_big(10, 1000, 0, 200, 48) == 48, "floor");
    CHECK(sel_uim_flow_big(-5, 1000, 0, 200, 48) == 48, "negative width");

    CHECK(sel_uim_flow_centre_q16(0, 4, 0) == 0, "glide start is from");
    CHECK(sel_uim_flow_centre_q16(0, 4, 32768) == 2 * 65536, "glide half");
    CHECK(sel_uim_flow_centre_q16(0, 4, 65536) == 4 * 65536, "at rest");

    n = sel_uim_flow_layout(10 * 65536, 0u, 100u, 200, 500, 20, c, SEL_UIM_FLOW_MAX);
    CHECK(n == 7u, "full stack is 7 covers, got %u", (unsigned)n);
    CHECK(c[n - 1u].g == 10u && c[n - 1u].sz == 200 && c[n - 1u].x == 400 && c[n - 1u].y == 20, "front cover last");
    for (i = 1u; i < n; i++) {
        CHECK(c[i - 1u].a >= c[i].a, "far to near at %u", (unsigned)i);
    }
    CHECK(c[0].g == 7u && c[1].g == 13u, "ties: left first (%u %u)", (unsigned)c[0].g, (unsigned)c[1].g);
    CHECK(c[0].x == 500 - 150 - 40, "far left x %d", (int)c[0].x);
    CHECK(c[1].x == 500 + 150 - 40, "far right x %d", (int)c[1].x);
    n = sel_uim_flow_layout(0, 0u, 100u, 200, 500, 20, c, SEL_UIM_FLOW_MAX);
    CHECK(n == 4u, "left edge clips: %u", (unsigned)n);
    n = sel_uim_flow_layout(10 * 65536 + 32768, 0u, 100u, 200, 500, 20, c, SEL_UIM_FLOW_MAX);
    CHECK(n <= (uint32_t)SEL_UIM_FLOW_MAX && n >= 7u, "glide count %u", (unsigned)n);
    n = sel_uim_flow_layout(10 * 65536, 8u, 3u, 200, 500, 20, c, SEL_UIM_FLOW_MAX);
    CHECK(n == 3u && c[2].g == 2u, "window start offsets g: n=%u", (unsigned)n);

    n = sel_uim_flow_layout(10 * 65536, 0u, 100u, 200, 500, 20, c, SEL_UIM_FLOW_MAX);
    CHECK(sel_uim_flow_hit(c, n, 500, 100, 1) == 10, "front hit");
    CHECK(sel_uim_flow_hit(c, n, 380, 100, 1) == -1, "side ignored when front only");
    CHECK(sel_uim_flow_hit(c, n, 380, 100, 0) == 9, "side hit: %d", (int)sel_uim_flow_hit(c, n, 380, 100, 0));
    CHECK(sel_uim_flow_hit(c, n, 345, 100, 0) == 8, "far side hit");
    CHECK(sel_uim_flow_hit(c, n, 5, 5, 0) == -1, "miss");

    if (g_fail == 0) {
        printf("uim_flow_ok\n");
    }
    return g_fail != 0;
}
