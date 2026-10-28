/* Layout tree and breakpoints of the desktop UI (plan DUI-P1-7; M4-P2-4). A canvas width picks one of four layouts;
 * each layout is a list of regions with rectangles that tile the canvas without overlap. */
#include <selecta/uimodel.h>

#include <stdio.h>
#include <string.h>

#define BAR_H 72
#define BAR_ONE_ROW_H 40 /* DUI-P4-20: the footer as one row on a short canvas */
#define BANNER_H 40
#define SIDEBAR_WIDE 220
#define SIDEBAR_NARROW 200
#define SIDEBAR_ICONS 56
#define SIDEBAR_EXPANDED_WIDE 360 /* I609: the widened Your Library sidebar, three-column layout */
#define SIDEBAR_EXPANDED_NARROW 320 /* two-column layout */
#define SIDEBAR_EXPANDED_ICONS 220 /* the icon rail opens to a labelled sidebar */
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
    if (width >= SEL_UIM_BP_THREE_COLUMN_MIN) {
        return SEL_UIM_BP_THREE_COLUMN;
    }
    if (width >= SEL_UIM_BP_TWO_COLUMN_MIN) {
        return SEL_UIM_BP_TWO_COLUMN;
    }
    if (width >= SEL_UIM_BP_ICON_SIDEBAR_MIN) {
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
    sel_uim_layout_compute_rail(l, width, height, tier, 0u);
}

/* I547: rail_w is the labelled rail width the caller measured from its font (0: SIDEBAR_RAIL_LABELLED). It is never
 * narrower than the default and never wider than a third of the window. */
void sel_uim_layout_compute_rail(sel_uim_layout* l, uint32_t width, uint32_t height, uint32_t tier, uint32_t rail_w) {
    sel_uim_layout_compute_bar(l, width, height, tier, rail_w, BAR_H);
}

/* DUI-P4-20: the player bar height for a canvas height: one row on a short canvas (SEL_UIM_FOOTER_ONE_ROW_MAX_H and below),
 * the two row bar otherwise. */
uint32_t sel_uim_footer_height(uint32_t height) {
    return height <= SEL_UIM_FOOTER_ONE_ROW_MAX_H ? BAR_ONE_ROW_H : BAR_H;
}

/* As compute_rail with the player bar height chosen by the caller (the desktop UI passes sel_uim_footer_height). */
void sel_uim_layout_compute_bar(sel_uim_layout* l, uint32_t width, uint32_t height, uint32_t tier, uint32_t rail_w,
                                uint32_t bar_height) {
    sel_uim_layout_compute_expand(l, width, height, tier, rail_w, bar_height, 0u);
}

/* I615: the top bar rects. The group (Home button, gap, field) is centred in the window, the field at most FIELD_MAX wide;
 * on a narrow canvas the field shrinks and the group keeps a gutter on both sides. The Browse button is the field's last
 * BUTTON wide slice, behind a divider drawn by the caller. */
static void topbar_set(sel_uim_region* r, const char* name, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    r->name = name;
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    r->min_w = 0;
    r->min_h = 0;
}

static void topbar_compute(sel_uim_layout* l, uint32_t width, uint32_t tb_h) {
    uint32_t btn = SEL_UIM_TOPBAR_BUTTON, gap = 8u, gutter = 16u;
    uint32_t fw = width > 2u * gutter + btn + gap ? width - 2u * gutter - btn - gap : 0u;
    uint32_t gw, x0;
    int32_t y = (int32_t)((tb_h - btn) / 2u);
    fw = fw > SEL_UIM_TOPBAR_FIELD_MAX ? SEL_UIM_TOPBAR_FIELD_MAX : fw;
    gw = btn + gap + fw;
    x0 = (width - gw) / 2u;
    topbar_set(&l->topbar, "topbar", 0, 0, width, tb_h);
    topbar_set(&l->topbar_home, "topbar-home", (int32_t)x0, y, btn, btn);
    topbar_set(&l->topbar_field, "topbar-field", (int32_t)(x0 + btn + gap), y, fw, btn);
    topbar_set(&l->topbar_browse, "topbar-browse", (int32_t)(x0 + btn + gap + fw - btn), y, btn, btn);
}

/* I609: as compute_bar; `expand` widens the sidebar (Spotify's Expand Library). The bitmap tier and the mini layout have no
 * wider form and ignore it. */
void sel_uim_layout_compute_expand(sel_uim_layout* l, uint32_t width, uint32_t height, uint32_t tier, uint32_t rail_w,
                                   uint32_t bar_height, uint32_t expand) {
    uint32_t bp = sel_uim_breakpoint_for_tier(width, tier);
    uint32_t bar_h = height > bar_height ? bar_height : height;
    uint32_t tb_h = (tier != SEL_UIM_TIER_BITMAP && bp != SEL_UIM_BP_MINI_SUGGESTION && height >= bar_h + SEL_UIM_TOPBAR_H + MAIN_MIN_H)
                        ? SEL_UIM_TOPBAR_H : 0u; /* I615: the default tier's top bar */
    uint32_t body_h = height - bar_h - tb_h;
    uint32_t side = 0, detail = 0, top = 0, main_w;
    memset(l, 0, sizeof *l);
    l->width = width;
    l->height = height;
    l->breakpoint = bp;
    if (bp == SEL_UIM_BP_THREE_COLUMN) {
        side = expand ? SIDEBAR_EXPANDED_WIDE : SIDEBAR_WIDE;
        detail = DETAIL_W;
    } else if (bp == SEL_UIM_BP_TWO_COLUMN) {
        side = expand ? SIDEBAR_EXPANDED_NARROW : SIDEBAR_NARROW;
    } else if (bp == SEL_UIM_BP_ICON_SIDEBAR) {
        if (tier == SEL_UIM_TIER_BITMAP) {
            side = rail_w > SIDEBAR_RAIL_LABELLED ? rail_w : SIDEBAR_RAIL_LABELLED;
            side = side > width / 3u ? (width / 3u > SIDEBAR_RAIL_LABELLED ? width / 3u : SIDEBAR_RAIL_LABELLED) : side;
        } else {
            side = expand ? SIDEBAR_EXPANDED_ICONS : SIDEBAR_ICONS;
        }
    } else {
        top = body_h > BANNER_H ? BANNER_H : body_h;
    }
    main_w = width - side - detail;
    if (tb_h > 0u) {
        topbar_compute(l, width, tb_h);
    }
    if (top > 0u) {
        add(l, "suggestion", width, 0, 0, 0, width, top);
    }
    if (side > 0u) {
        add(l, "sidebar", side, MAIN_MIN_H, 0, (int32_t)tb_h, side, body_h);
    }
    add(l, "main", MAIN_MIN_W, MAIN_MIN_H, (int32_t)side, (int32_t)(tb_h + top), main_w, body_h - top);
    if (detail > 0u) {
        add(l, "detail", detail, MAIN_MIN_H, (int32_t)(side + main_w), (int32_t)tb_h, detail, body_h);
    }
    add(l, "player", 0, 0, 0, (int32_t)(tb_h + body_h), width, bar_h);
}

/* I668: puts the sidebar at `side` wide mid-tween and gives the main region the rest, the detail column and the player staying put.
 * The sidebar keeps its minimum only down to the width it has now. */
void sel_uim_layout_set_side_width(sel_uim_layout* l, uint32_t side) {
    sel_uim_region *sb = NULL, *mn = NULL;
    uint32_t i, old;
    for (i = 0; i < l->count; i++) {
        if (strcmp(l->regions[i].name, "sidebar") == 0) {
            sb = &l->regions[i];
        } else if (strcmp(l->regions[i].name, "main") == 0) {
            mn = &l->regions[i];
        }
    }
    if (sb == NULL || mn == NULL) {
        return;
    }
    old = sb->w;
    sb->w = side;
    sb->min_w = side < sb->min_w ? side : sb->min_w;
    mn->x = (int32_t)side;
    mn->w = mn->w + old - side;
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
