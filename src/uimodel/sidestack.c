/* I1222e: the sidebar stack, see include/selecta/uimodel/sidestack.h. */
#include <selecta/uimodel/doors.h>
#include <selecta/uimodel/sidestack.h>

static uint32_t push(sel_uim_side_item* out, uint32_t max, uint32_t n, sel_uim_side_kind kind, int32_t y, int32_t h, int32_t door) {
    if (n < max) {
        out[n].kind = kind;
        out[n].y = y;
        out[n].h = h;
        out[n].door = door;
    }
    return n + 1u;
}

/* The height of everything but the brand and the list: the shown doors, the header, the chips and the tools. */
static int32_t fixed_h(const sel_uim_side_spec* s) {
    int32_t t = 0;
    uint32_t i;
    for (i = 0; i < SEL_UIM_DOOR_COUNT; i++) {
        t += sel_uim_door_shown_in(i, s->top_bar, !s->rail) ? s->door_h : 0;
    }
    if (!s->rail) {
        t += s->libhdr_h + (s->chip_rows > 0 ? s->chip_rows * s->chip_row_h : 0) + s->tools_h;
    }
    return t;
}

uint32_t sel_uim_side_stack(const sel_uim_side_spec* s, int32_t w, int32_t h, sel_uim_side_item* out, uint32_t max) {
    uint32_t n = 0, i;
    int32_t y = 0, rest, chips_h;
    int brand = w >= s->brand_min_w && s->brand_h > 0;
    if (brand && s->list_row_h > 0 && !s->rail && h - s->brand_h - fixed_h(s) < SEL_UIM_SIDE_MIN_LIST_ROWS * s->list_row_h) {
        brand = 0; /* short height: the list keeps its rows, the brand goes (decision 4) */
    }
    if (brand) {
        n = push(out, max, n, SEL_UIM_SIDE_BRAND, y, s->brand_h, -1);
        y += s->brand_h;
    }
    for (i = 0; i < SEL_UIM_DOOR_COUNT; i++) {
        if (sel_uim_door_shown_in(i, s->top_bar, !s->rail)) {
            n = push(out, max, n, SEL_UIM_SIDE_DOOR, y, s->door_h, (int32_t)i);
            y += s->door_h;
        }
    }
    if (s->rail) { /* the icon rail: doors only */
        return n < max ? n : max;
    }
    n = push(out, max, n, SEL_UIM_SIDE_LIBHDR, y, s->libhdr_h, -1);
    y += s->libhdr_h;
    chips_h = s->chip_rows > 0 ? s->chip_rows * s->chip_row_h : 0;
    if (chips_h > 0) {
        n = push(out, max, n, SEL_UIM_SIDE_CHIPS, y, chips_h, -1);
        y += chips_h;
    }
    if (s->tools_h > 0) {
        n = push(out, max, n, SEL_UIM_SIDE_TOOLS, y, s->tools_h, -1);
        y += s->tools_h;
    }
    rest = h - y;
    n = push(out, max, n, SEL_UIM_SIDE_LIST, y, rest > 0 ? rest : 0, -1);
    return n < max ? n : max;
}
