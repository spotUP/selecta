/* I1222c: the text tier's layout (SEL_UIM_TIER_TEXT) in cells, per thoughts/shared/research/2026-11-09-text-frame-design.md sections 3
 * and 7: three columns from 120 cols (sidebar 26, detail 34), two columns from 80 cols (sidebar 20, no detail), an icon rail from 60 cols, the
 * player alone below, a 1 row top bar, the player 2 rows (1 row at 25 rows and below), no hints region. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

static int g_fail;

static const sel_uim_region* reg(const sel_uim_layout* l, const char* name) {
    uint32_t i;
    for (i = 0; i < l->count; i++) {
        if (strcmp(l->regions[i].name, name) == 0) {
            return &l->regions[i];
        }
    }
    return NULL;
}

/* name == NULL case: expects the region absent. */
static void want(const sel_uim_layout* l, const char* tag, const char* name, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    const sel_uim_region* r = reg(l, name);
    if (r == NULL) {
        printf("FAIL %s: region %s missing\n", tag, name);
        g_fail++;
    } else if (r->x != x || r->y != y || r->w != w || r->h != h) {
        printf("FAIL %s: %s is %d,%d %ux%u, want %d,%d %ux%u\n", tag, name, r->x, r->y, r->w, r->h, x, y, w, h);
        g_fail++;
    }
}

static void absent(const sel_uim_layout* l, const char* tag, const char* name) {
    if (reg(l, name) != NULL) {
        printf("FAIL %s: region %s present\n", tag, name);
        g_fail++;
    }
}

static void checked(const sel_uim_layout* l, const char* tag) {
    char msg[96];
    if (sel_uim_layout_check(l, msg, sizeof msg) != 0) {
        printf("FAIL %s: layout_check: %s\n", tag, msg);
        g_fail++;
    }
    absent(l, tag, "hints");
}

static void size(const char* tag, uint32_t cols, uint32_t rows, uint32_t expand, uint32_t hidden, uint32_t bp, uint32_t side,
                 uint32_t main_w, uint32_t detail, uint32_t tb, uint32_t player, uint32_t main_h) {
    sel_uim_layout l;
    if (sel_uim_layout_compute_text(&l, cols, rows, expand, hidden) != 0 || l.breakpoint != bp) {
        printf("FAIL %s: breakpoint %u, want %u\n", tag, l.breakpoint, bp);
        g_fail++;
    }
    checked(&l, tag);
    if (side > 0u) {
        want(&l, tag, "sidebar", 0, 0, side, rows - player);
    } else {
        absent(&l, tag, "sidebar");
    }
    if (main_w > 0u) {
        want(&l, tag, "main", (int32_t)side, (int32_t)tb, main_w, main_h);
    } else {
        absent(&l, tag, "main");
    }
    if (detail > 0u) {
        want(&l, tag, "detail", (int32_t)(side + main_w), (int32_t)tb, detail, main_h);
    } else {
        absent(&l, tag, "detail");
    }
    want(&l, tag, "player", 0, (int32_t)(rows - player), cols, player);
    if (l.topbar.h != tb || (tb > 0u && (l.topbar.x != (int32_t)side || l.topbar.w != cols - side || l.topbar.y != 0))) {
        printf("FAIL %s: topbar %d,%d %ux%u\n", tag, l.topbar.x, l.topbar.y, l.topbar.w, l.topbar.h);
        g_fail++;
    }
}

int main(void) {
    uint32_t c, r;
    sel_uim_layout l;
    const uint32_t T = SEL_UIM_BP_THREE_COLUMN, W = SEL_UIM_BP_TWO_COLUMN, I = SEL_UIM_BP_ICON_SIDEBAR, M = SEL_UIM_BP_MINI_SUGGESTION;
    /*   tag               cols rows exp hid bp  side main detail tb player main_h */
    size("160x50",         160, 50, 0, 0, T, 26, 100, 34, 1, 2, 47);
    size("120x40",         120, 40, 0, 0, T, 26, 60, 34, 1, 2, 37);
    size("120x40 hidden",  120, 40, 0, 1, T, 26, 94, 0, 1, 2, 37);
    size("120x40 expanded", 120, 40, 1, 0, T, 48, 40, 32, 1, 2, 37);
    size("100x30",         100, 30, 0, 0, W, 20, 80, 0, 1, 2, 27);
    size("80x25",          80, 25, 0, 0, W, 20, 60, 0, 1, 1, 23);
    size("80x24",          80, 24, 0, 0, W, 20, 60, 0, 1, 1, 22);
    size("80x32 Amiga",    80, 32, 0, 0, W, 20, 60, 0, 1, 2, 29);
    size("80x25 hidden",   80, 25, 0, 1, W, 20, 60, 0, 1, 1, 23);
    size("80x25 expanded", 80, 25, 1, 0, W, 32, 48, 0, 1, 1, 23);
    size("60x24",          60, 24, 0, 0, I, 3, 57, 0, 1, 1, 22);
    size("50x20",          50, 20, 0, 0, M, 0, 0, 0, 0, 1, 0);
    /* The layout check holds at every size from 40x12 to 200x60, expanded or not, detail shown or hidden. */
    for (c = 40; c <= 200; c++) {
        for (r = 12; r <= 60; r++) {
            uint32_t e;
            for (e = 0; e < 4; e++) {
                char tag[32];
                snprintf(tag, sizeof tag, "%ux%u e%u", c, r, e);
                if (sel_uim_layout_compute_text(&l, c, r, e & 1u, e >> 1) != 0) {
                    printf("FAIL %s: compute failed\n", tag);
                    g_fail++;
                }
                checked(&l, tag);
                if (g_fail > 20) {
                    return 1;
                }
            }
        }
    }
    /* The tier table: text counts cells and has no banner; the other tiers keep their pixel values. */
    if (sel_uim_metrics_for(SEL_UIM_TIER_TEXT)->banner_h != 0 || sel_uim_metrics_for(SEL_UIM_TIER_DEFAULT)->bar_h != 72 ||
        sel_uim_metrics_for(SEL_UIM_TIER_BITMAP)->detail_w != 320) {
        printf("FAIL metrics table\n");
        g_fail++;
    }
    if (g_fail != 0) {
        return 1;
    }
    printf("uim_layout_text_ok\n");
    return 0;
}
