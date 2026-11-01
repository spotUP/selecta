/* I671/I949b: the screen plan and the open ladder. MUI 3.8 refuses a window whose minimum size does not fit the screen
 * (MUIA_Window_Open reads back 0). The window minimum comes from MUI itself (MUIM_AskMinMax runs inside the open attempt),
 * not from a height model kept here: the open attempt is the measurement. The ladder names the layout to try at each step:
 * the layout of the screen first, then the most compact class; mui_ui.c reopens the SAME window object after each refusal
 * (rig-proven on MUI 3.8, no dispose, no rebuild) and reports the failure after the last step. Selecta never picks an
 * interlaced screen on its own; a user-chosen laced screen is laid out with the 256-row metrics (half its rows). Pure C99:
 * the host tests the plan and the ladder, mui_ui.c applies them. */
#include <string.h>

#include "mui_icons.h"
#include "mui_map.h"

static uint32_t g_plans, g_ladder_calls;

uint32_t mui_button_height(void) {
    return (uint32_t)mui_model_icon_px() + 2u * MUI_BTN_PAD_Y + MUI_BTN_FRAME;
}

uint32_t mui_open_ladder(uint32_t step, uint32_t screen_w, uint32_t screen_h, int laced, mui_open_step_t* out) {
    uint32_t rows = laced ? screen_h / 2u : screen_h;
    g_ladder_calls++;
    if (out == NULL || step >= MUI_OPEN_STEPS) {
        return 0;
    }
    out->layout_w = screen_w;
    out->layout_h = rows;
    if (step == 1u) {
        if (screen_w <= MUI_COMPACT_W) {
            return 0; /* already compact: nothing more compact to try */
        }
        out->layout_w = MUI_COMPACT_W;
    }
    out->step = step;
    return 1;
}

uint32_t mui_open_ladder_calls(void) {
    return g_ladder_calls;
}

uint32_t mui_screen_plan(uint32_t wb_mode, uint32_t wb_w, uint32_t wb_h, int foreign, mui_screen_plan_t* out) {
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof *out);
    g_plans++;
    out->mode = wb_mode;
    out->width = wb_w;
    out->height = wb_h;
    out->depth = foreign ? 8u : 4u; /* RTG: no chip RAM bitplanes, 256 colours cost nothing; AGA: 16 pens (section 4) */
    return 0;
}

uint32_t mui_screen_plans(void) {
    return g_plans;
}

int mui_screen_is_rtg(int dipf_foreign, int bm_standard, uint32_t bm_depth) {
    return dipf_foreign || !bm_standard || bm_depth > 8u;
}

static uint32_t g_attr_calls;

void mui_window_attrs(int own_screen, mui_window_attrs_t* out) {
    if (out == NULL) {
        return;
    }
    g_attr_calls++;
    out->borderless = out->backdrop = (uint8_t)(own_screen != 0);
    out->dragbar = out->depthgadget = out->sizegadget = out->closegadget = out->titled = (uint8_t)(own_screen == 0);
}

uint32_t mui_window_attrs_calls(void) {
    return g_attr_calls;
}
