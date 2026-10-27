/* mui-viz-surface (MUI-P5-3) and mui-budget (MUI-P5-5), plan 2026-10-25-amiga-mui.md.
 * Usage: mui_viz_test surface|budget */
#include <stdio.h>
#include <string.h>

#include "mui_copper.h"
#include "mui_viz.h"

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

static uint32_t fake_calls;
static uint32_t fake_fmt_ok, fake_pitch_ok, fake_colors_ok;

/* Stands in for a viz plugin's VIZ_FRAME hook: asserts what the host hands over, draws one primary run. */
static uint32_t fake_render(const sel_viz_frame* f, sel_surface* s, const sel_viz_colors* c) {
    uint32_t x;
    (void)f;
    fake_calls++;
    fake_fmt_ok = s->fmt == SEL_PIX_INDEX8;
    fake_pitch_ok = s->pitch >= s->w;
    fake_colors_ok = c->primary == MUI_VIZ_PRIMARY && c->background == MUI_VIZ_BACKGROUND;
    for (x = 0; x < s->w; x++) {
        ((uint8_t*)s->pixels)[x] = (uint8_t)c->primary;
    }
    return 1u;
}

static int test_surface(void) {
    static const sel_tag hooks[] = {{SEL_VIZ_FRAME, (uintptr_t)fake_render}, {SEL_TAG_END, 0}};
    static const sel_tag desc[] = {{SEL_PLUGIN_TAGS, (uintptr_t)hooks}, {SEL_TAG_END, 0}};
    static const sel_tag bare[] = {{SEL_TAG_END, 0}};
    mui_viz v;
    sel_viz_frame f;
    uint32_t px[7 * 3];
    memset(&v, 0, sizeof v);
    memset(&f, 0, sizeof f);
    CHECK(mui_viz_bind(&v, bare) == 0, "descriptor without hook binds");
    CHECK(mui_viz_bind(&v, desc) == 1 && v.fn == fake_render, "hook not found");
    CHECK(mui_viz_fit(&v, 0, 3) == 0, "empty size accepted");
    CHECK(mui_viz_fit(&v, 7, 3) == 1, "fit 7x3");
    CHECK(v.surf.fmt == SEL_PIX_INDEX8 && v.surf.pitch >= v.surf.w, "surface fmt/pitch");
    v.palette[MUI_VIZ_BACKGROUND] = 0xFF000000u;
    v.palette[MUI_VIZ_PRIMARY] = 0xFF00FF00u;
    CHECK(mui_viz_render(&v, &f) == 1u && fake_calls == 1u && v.frames == 1u, "render did not run");
    CHECK(fake_fmt_ok && fake_pitch_ok && fake_colors_ok, "plugin saw a wrong surface or colours");
    mui_viz_expand(&v, px);
    CHECK(px[0] == 0xFF00FF00u && px[6] == 0xFF00FF00u && px[7] == 0xFF000000u, "expand through palette");
    mui_viz_release(&v);
    CHECK(mui_viz_render(&v, &f) == 0u && fake_calls == 1u, "released surface still renders");
    return failures;
}

/* Redraws allowed in one simulated second of 5 ms UI ticks. */
static uint32_t per_second(const sel_uim_budget* b, mui_redraw_kind k) {
    mui_gate g = {0, 0};
    uint32_t t, n = 0;
    for (t = 1000; t < 2000; t += 5) {
        n += (uint32_t)mui_gate_due(&g, mui_budget_interval_ms(b, k), t);
    }
    return n;
}

static int test_budget(void) {
    sel_uim_budget rgba, idx;
    CHECK(sel_uim_budget_for(&rgba, "rgba32") && sel_uim_budget_for(&idx, "index8"), "budgets");
    CHECK(per_second(&rgba, MUI_REDRAW_VIZ) == 25u, "rgba viz %u", per_second(&rgba, MUI_REDRAW_VIZ));
    CHECK(per_second(&rgba, MUI_REDRAW_GRID) == 25u, "rgba grid %u", per_second(&rgba, MUI_REDRAW_GRID));
    CHECK(per_second(&idx, MUI_REDRAW_VIZ) == 10u, "index8 viz %u", per_second(&idx, MUI_REDRAW_VIZ));
    CHECK(per_second(&idx, MUI_REDRAW_GRID) == 5u, "index8 grid %u", per_second(&idx, MUI_REDRAW_GRID));
    return failures;
}

/* mui-copper-tint (MUI-P5-8): row colours to copper entries, grid switch on and off. */
static int test_copper(void) {
    uint32_t px[4 * 4], rows[4];
    mui_copper_entry e[8];
    int i;
    for (i = 0; i < 16; i++) {
        px[i] = (i / 4 < 2) ? 0xFFFF0000u : 0xFF0000FFu; /* two red rows, two blue rows */
    }
    px[0] = 0xFF000000u; /* row 0 average: (0 + 3 * 255) / 4 = 191 red */
    mui_copper_row_colors(px, 4, 4, rows);
    CHECK(rows[0] == 0xBF0000u && rows[1] == 0xFF0000u && rows[3] == 0x0000FFu, "row colours %06x %06x %06x", rows[0], rows[1], rows[3]);
    CHECK(mui_copper_cover_list(rows, 4, 10, e, 8) == 3u, "entries merge equal lines");
    CHECK(e[0].y == 10 && e[0].rgb4 == 0xB00u && e[1].y == 11 && e[1].rgb4 == 0xF00u && e[2].y == 12 && e[2].rgb4 == 0x00Fu, "entry values");
    CHECK(mui_copper_grid_row_list(1, rows, 4, 20, e, 8) == 3u && e[0].y == 20, "grid tint on");
    CHECK(mui_copper_grid_row_list(0, rows, 4, 20, e, 8) == 0u, "grid tint off adds nothing");
    CHECK(mui_copper_cover_list(rows, 4, 0, e, 2) == 2u, "cap respected");
    return failures;
}

int main(int argc, char** argv) {
    int r = 1;
    if (argc > 1 && strcmp(argv[1], "surface") == 0) {
        r = test_surface();
    } else if (argc > 1 && strcmp(argv[1], "copper") == 0) {
        r = test_copper();
    } else if (argc > 1 && strcmp(argv[1], "budget") == 0) {
        r = test_budget();
    }
    printf("%s\n", r == 0 ? "PASS" : "FAILED");
    return r == 0 ? 0 : 1;
}
