/* I1473g8b1: shared Cover Flow geometry (moved from plugins/ui/desktop/desktop_ui.c, integer maths unchanged). */
#include <selecta/uimodel/flow.h>

enum { STEP_DIV = 4, SHRINK_DIV = 5 };
static const uint8_t k_scrim[SEL_UIM_FLOW_SIDES + 1] = {0u, 90u, 150u, 150u};

static int32_t off_of(int32_t big, int32_t k) { return k * big / STEP_DIV; }
static int32_t size_of(int32_t big, int32_t k) { return big - k * big / SHRINK_DIV; }

int32_t sel_uim_flow_stack_w(int32_t big) { return 2 * (off_of(big, SEL_UIM_FLOW_SIDES) + size_of(big, SEL_UIM_FLOW_SIDES) / 2); }

int32_t sel_uim_flow_big(int32_t avail_w, int32_t area_h, int32_t capt, int32_t ref, int32_t floor) {
    int32_t fit_w = ref * (avail_w > 0 ? avail_w : 0) / sel_uim_flow_stack_w(ref), fit_h = area_h - capt;
    int32_t big = fit_w < fit_h ? fit_w : fit_h;
    return big > floor ? big : floor;
}

void sel_uim_flow_slot(int32_t big, int32_t a, int32_t* sz, int32_t* off, int32_t* alpha) {
    int32_t k = a >> 16, f = a & 0xFFFF;
    if (k >= SEL_UIM_FLOW_SIDES) {
        *sz = (int32_t)((int64_t)size_of(big, SEL_UIM_FLOW_SIDES) * (65536 - f) >> 16);
        *off = off_of(big, SEL_UIM_FLOW_SIDES);
        *alpha = k_scrim[SEL_UIM_FLOW_SIDES];
        return;
    }
    *sz = size_of(big, k) + (int32_t)((int64_t)(size_of(big, k + 1) - size_of(big, k)) * f >> 16);
    *off = off_of(big, k) + (int32_t)((int64_t)(off_of(big, k + 1) - off_of(big, k)) * f >> 16);
    *alpha = (int32_t)k_scrim[k] + (int32_t)((int64_t)((int32_t)k_scrim[k + 1] - (int32_t)k_scrim[k]) * f >> 16);
}

int64_t sel_uim_flow_centre_q16(int64_t from_q16, uint32_t cursor, int32_t glide_q16) {
    int64_t to = (int64_t)cursor * 65536;
    return glide_q16 < 65536 ? to + (((from_q16 - to) * (65536 - glide_q16)) >> 16) : to;
}

uint32_t sel_uim_flow_layout(int64_t c, uint32_t window_start, uint32_t row_n, int32_t big, int32_t cx, int32_t y0,
                             sel_uim_flow_cover* out, uint32_t cap) {
    int64_t g0 = (c >> 16) - SEL_UIM_FLOW_SIDES, g;
    int32_t sgn[SEL_UIM_FLOW_MAX];
    uint32_t n = 0u, i;
    if (cap > SEL_UIM_FLOW_MAX) {
        cap = SEL_UIM_FLOW_MAX;
    }
    /* Candidates |p| < SIDES + 1, insertion sorted by |p| descending (stable: the left one of a tie first). */
    for (g = g0 - 1; g <= g0 + 2 * SEL_UIM_FLOW_SIDES + 2 && n < cap; g++) {
        int64_t p = g * 65536 - c;
        int32_t a = (int32_t)(p < 0 ? -p : p);
        if (g < 0 || g < (int64_t)window_start || g >= (int64_t)window_start + (int64_t)row_n || a >= (SEL_UIM_FLOW_SIDES + 1) * 65536) {
            continue;
        }
        for (i = n; i > 0u && out[i - 1u].a < a; i--) {
            out[i] = out[i - 1u];
            sgn[i] = sgn[i - 1u];
        }
        out[i].g = (uint32_t)(g - (int64_t)window_start);
        out[i].a = a;
        sgn[i] = p < 0 ? -1 : 1;
        n++;
    }
    for (i = 0u; i < n; i++) {
        int32_t sz, off, alpha;
        sel_uim_flow_slot(big, out[i].a, &sz, &off, &alpha);
        out[i].sz = sz;
        out[i].alpha = alpha;
        out[i].x = cx + sgn[i] * off - sz / 2;
        out[i].y = y0 + (big - sz) / 2;
    }
    return n;
}

int32_t sel_uim_flow_hit(const sel_uim_flow_cover* c, uint32_t n, int32_t x, int32_t y, int front_only) {
    uint32_t i = n, stop = front_only && n > 0u ? n - 1u : 0u;
    while (i > stop) {
        i--;
        if (x >= c[i].x && x < c[i].x + c[i].sz && y >= c[i].y && y < c[i].y + c[i].sz) {
            return (int32_t)c[i].g;
        }
    }
    return -1;
}
