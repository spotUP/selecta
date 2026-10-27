/* Layout tree and breakpoints of the desktop UI (plan DUI-P1-7; M4-P2-4). A canvas width picks one of four layouts;
 * each layout is a list of regions with rectangles that tile the canvas without overlap. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

#define BAR_H 72
#define BANNER_H 40
#define SIDEBAR_WIDE 220
#define SIDEBAR_NARROW 200
#define SIDEBAR_ICONS 56
#define SIDEBAR_RAIL_LABELLED 160 /* I526 D1: the bitmap tier rail, mini8 labels beside the icon */
#define DETAIL_W 320
#define MAIN_MIN_W 240
#define MAIN_MIN_H 120

static const char* const k_layout_names[SEL_UIM_BP_COUNT] = {"three-column", "two-column", "icon-sidebar",
                                                             "mini-suggestion"};

const char* sel_uim_breakpoint_name(uint32_t bp) { return bp < SEL_UIM_BP_COUNT ? k_layout_names[bp] : ""; }

uint32_t sel_uim_breakpoint_for_tier(uint32_t width, uint32_t tier) {
    if (tier == SEL_UIM_TIER_BITMAP && width >= 600u && width < 700u) {
        return SEL_UIM_BP_ICON_SIDEBAR; /* I526: the 640 wide native screen keeps a rail, not the mini suggestion */
    }
    return sel_uim_breakpoint_for(width);
}

uint32_t sel_uim_breakpoint_for(uint32_t width) {
    if (width >= 1200u) {
        return SEL_UIM_BP_THREE_COLUMN;
    }
    if (width >= 900u) {
        return SEL_UIM_BP_TWO_COLUMN;
    }
    if (width >= 700u) {
        return SEL_UIM_BP_ICON_SIDEBAR;
    }
    return SEL_UIM_BP_MINI_SUGGESTION;
}

static void add(sel_uim_layout* l, const char* name, uint32_t min_w, uint32_t min_h, int32_t x, int32_t y, uint32_t w,
                uint32_t h) {
    sel_uim_region* r = &l->regions[l->count++];
    r->name = name;
    r->min_w = min_w;
    r->min_h = min_h;
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

void sel_uim_layout_compute(sel_uim_layout* l, uint32_t width, uint32_t height) {
    sel_uim_layout_compute_tier(l, width, height, SEL_UIM_TIER_DEFAULT);
}

void sel_uim_layout_compute_tier(sel_uim_layout* l, uint32_t width, uint32_t height, uint32_t tier) {
    uint32_t bp = sel_uim_breakpoint_for_tier(width, tier);
    uint32_t bar_h = height > BAR_H ? BAR_H : height;
    uint32_t body_h = height - bar_h;
    uint32_t side = 0, detail = 0, top = 0, main_w;
    memset(l, 0, sizeof *l);
    l->width = width;
    l->height = height;
    l->breakpoint = bp;
    if (bp == SEL_UIM_BP_THREE_COLUMN) {
        side = SIDEBAR_WIDE;
        detail = DETAIL_W;
    } else if (bp == SEL_UIM_BP_TWO_COLUMN) {
        side = SIDEBAR_NARROW;
    } else if (bp == SEL_UIM_BP_ICON_SIDEBAR) {
        side = tier == SEL_UIM_TIER_BITMAP ? SIDEBAR_RAIL_LABELLED : SIDEBAR_ICONS;
    } else {
        top = body_h > BANNER_H ? BANNER_H : body_h;
    }
    main_w = width - side - detail;
    if (top > 0u) {
        add(l, "suggestion", width, 0, 0, 0, width, top);
    }
    if (side > 0u) {
        add(l, "sidebar", side, MAIN_MIN_H, 0, 0, side, body_h);
    }
    add(l, "main", MAIN_MIN_W, MAIN_MIN_H, (int32_t)side, (int32_t)top, main_w, body_h - top);
    if (detail > 0u) {
        add(l, "detail", detail, MAIN_MIN_H, (int32_t)(side + main_w), 0, detail, body_h);
    }
    add(l, "player", 0, 0, 0, (int32_t)body_h, width, bar_h);
}

/* 0 when every region lies inside the canvas, meets its minimum size and overlaps no other region. */
int sel_uim_layout_check(const sel_uim_layout* l, char* msg, size_t cap) {
    uint32_t i, j;
    msg[0] = '\0';
    for (i = 0; i < l->count; i++) {
        const sel_uim_region* a = &l->regions[i];
        if (a->x < 0 || a->y < 0 || (uint32_t)a->x + a->w > l->width || (uint32_t)a->y + a->h > l->height) {
            snprintf(msg, cap, "region %s outside the canvas", a->name);
            return 1;
        }
        if (a->w < a->min_w || a->h < a->min_h) {
            snprintf(msg, cap, "region %s below its minimum size", a->name);
            return 1;
        }
        for (j = i + 1u; j < l->count; j++) {
            const sel_uim_region* b = &l->regions[j];
            if (a->x < b->x + (int32_t)b->w && b->x < a->x + (int32_t)a->w && a->y < b->y + (int32_t)b->h &&
                b->y < a->y + (int32_t)a->h) {
                snprintf(msg, cap, "regions %s and %s overlap", a->name, b->name);
                return 1;
            }
        }
    }
    return 0;
}

int sel_uim_hit(const sel_uim_layout* l, int32_t x, int32_t y) {
    uint32_t i;
    for (i = 0; i < l->count; i++) {
        const sel_uim_region* r = &l->regions[i];
        if (x >= r->x && y >= r->y && x < r->x + (int32_t)r->w && y < r->y + (int32_t)r->h) {
            return (int)i;
        }
    }
    return -1;
}
