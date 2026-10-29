/* I671: the screen the MUI window needs. MUI 3.8 refuses a window whose minimum size does not fit the screen
 * (MUIA_Window_Open reads back 0), and the Selecta layout is taller than a 640x256 AGA Workbench: the height model below
 * sums the rows mui_ui.c builds, from the same constants the width model uses (mui_icons.h). When the Workbench screen is a
 * native (non-foreign) mode that is too short and not yet interlaced, the plan opens the own SELECTA screen in the laced
 * variant of that mode (PAL hires 640x256 becomes PAL hires laced 640x512), so the full layout opens full screen (D-M4-3)
 * instead of falling back to the text UI. Pure C99: the host tests the plan and the model, mui_ui.c applies them. */
#include <string.h>

#include "mui_icons.h"
#include "mui_map.h"

static uint32_t g_plans, g_laced;

uint32_t mui_button_height(void) {
    return (uint32_t)mui_model_icon_px() + 2u * MUI_BTN_PAD_Y + MUI_BTN_FRAME;
}

/* The mini layout (the breakpoint a 640 px screen gets: sidebar and detail hidden, suggestion hidden until the resize
 * check) stacks, top to bottom: the tool group (string row, two icon button rows, framed status text), the list (title row
 * plus MUI_LIST_MIN_ROWS entries), the player (two transport rows, the extras row, the seek row, the title row). Between
 * every two shown children of a group MUI puts MUI_GROUP_SPACING; the window adds its title bar and borders. */
uint32_t mui_layout_min_height(void) {
    uint32_t tools = MUI_STRING_H + 2u * mui_button_height() + MUI_TEXT_FRAMED_H + 3u * MUI_GROUP_SPACING;
    uint32_t list = (1u + MUI_LIST_MIN_ROWS) * MUI_FONT_H + MUI_BTN_FRAME;
    uint32_t player = 3u * mui_button_height() + MUI_SLIDER_H + MUI_FONT_H + 4u * MUI_GROUP_SPACING;
    uint32_t main_group = tools + list + MUI_GROUP_SPACING;
    return MUI_WIN_CHROME_H + main_group + player + MUI_GROUP_SPACING;
}

uint32_t mui_screen_plan(uint32_t wb_mode, uint32_t wb_w, uint32_t wb_h, int foreign, uint32_t need_h, mui_screen_plan_t* out) {
    if (out == NULL) {
        return 0;
    }
    memset(out, 0, sizeof *out);
    g_plans++;
    out->mode = wb_mode;
    out->width = wb_w;
    out->height = wb_h;
    out->depth = foreign ? 8u : 4u; /* RTG: no chip RAM bitplanes, 256 colours cost nothing; AGA: 16 pens (section 4) */
    if (foreign || wb_mode == MUI_INVALID_MODE || wb_h >= need_h || (wb_mode & MUI_MODE_LACE) != 0u) {
        return 0;
    }
    out->mode = wb_mode | MUI_MODE_LACE;
    out->height = wb_h * 2u;
    out->laced = 1;
    g_laced++;
    return 1;
}

uint32_t mui_screen_plans(void) {
    return g_plans;
}

uint32_t mui_screen_laced(void) {
    return g_laced;
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
