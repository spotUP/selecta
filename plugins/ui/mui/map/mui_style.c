#include "mui_map.h"

#include <selecta/engine.h>

uint32_t mui_style_mask(int screen_is_rtg) {
    return screen_is_rtg ? SEL_VIEW_STYLES_ALL : SEL_VIEW_STYLES_ALL & ~SEL_VIEW_STYLE_BIT(SEL_VIEW_STYLE_COVERFLOW);
}

#include <stdio.h>
#include <string.h>

#include <selecta/uimodel.h>

/* I1174: library styles on the MUI list. The engine keeps one flat row list for Grid, Cover Flow and Compact (engine.h); the desktop
 * draws them as tiles, a strip and dense rows. MUI 3.8 has no tile view, so the same rows are packed into List rows: Grid puts
 * several cells in one row (cell width from the desktop's g_tile_steps through sel_uim_tile_step), Cover Flow is one row with the
 * cursor cell between its neighbours, Compact is one dense label per row. Art is drawn by the RTG tier only (the planar tier has no
 * Cover Flow: written exemption in mui_style_mask and the I1174 row). */
static uint32_t g_style_packs;
uint32_t mui_style_packs(void) { return g_style_packs; }

uint32_t mui_style_kind(uint32_t style) {
    switch (style) {
    case SEL_VIEW_STYLE_GRID: return MUI_STYLE_GRID;
    case SEL_VIEW_STYLE_COVERFLOW: return MUI_STYLE_COVERFLOW;
    case SEL_VIEW_STYLE_COMPACT: return MUI_STYLE_COMPACT;
    default: return MUI_STYLE_LIST;
    }
}

uint32_t mui_style_cell_chars(uint32_t step) {
    int32_t px = sel_uim_tile_step(step < SEL_UIM_TILE_STEPS ? step : SEL_UIM_TILE_STEPS - 1u);
    return (uint32_t)(px / 8);
}

uint32_t mui_style_grid_cols(uint32_t list_chars, uint32_t step) {
    uint32_t cw = mui_style_cell_chars(step) + 1u, cols = list_chars / cw;
    return cols == 0u ? 1u : cols;
}

uint32_t mui_tile_step_move(uint32_t step, int dir) {
    if (dir > 0) {
        return step + 1u < SEL_UIM_TILE_STEPS ? step + 1u : step;
    }
    return step > 0u && dir < 0 ? step - 1u : step;
}

static void cell_put(char* out, size_t cap, size_t* used, const char* label, uint32_t width, int mark) {
    uint32_t i;
    int ended = 0;
    if (*used + width + 3u >= cap) {
        return;
    }
    out[(*used)++] = mark ? '[' : ' ';
    for (i = 0; i < width; i++) {
        ended = ended || label[i] == '\0';
        out[(*used)++] = ended ? ' ' : label[i];
    }
    out[(*used)++] = mark ? ']' : ' ';
    out[*used] = '\0';
}

uint32_t mui_style_pack(uint32_t style, uint32_t cols, uint32_t cell_chars, const mui_list_entry* in, uint32_t n, int32_t cursor,
                        mui_list_entry* out, uint32_t cap) {
    uint32_t kind = mui_style_kind(style), r, c, rows = 0;
    if (kind == MUI_STYLE_LIST || cap == 0u || n == 0u) {
        return 0;
    }
    g_style_packs++;
    if (cell_chars > 40u) {
        cell_chars = 40u;
    }
    if (kind == MUI_STYLE_COVERFLOW) {
        size_t used = 0;
        int32_t k, cur = cursor >= 0 && (uint32_t)cursor < n ? cursor : 0;
        memset(&out[0], 0, sizeof out[0]);
        out[0].index = in[cur].index;
        for (k = cur - 2; k <= cur + 2; k++) {
            if (k >= 0 && (uint32_t)k < n) {
                cell_put(out[0].text, sizeof out[0].text, &used, in[k].label, k == cur ? cell_chars : cell_chars / 2u, k == cur);
            }
        }
        snprintf(out[0].label, sizeof out[0].label, "%s", in[cur].label);
        return 1;
    }
    if (kind == MUI_STYLE_COMPACT) {
        for (r = 0; r < n && r < cap; r++) {
            out[r] = in[r];
            snprintf(out[r].text, sizeof out[r].text, "%s", in[r].label); /* no indent, no count: one dense label */
        }
        return r;
    }
    if (cols == 0u) {
        cols = 1u;
    }
    for (r = 0; r * cols < n && r < cap; r++) {
        size_t used = 0;
        memset(&out[r], 0, sizeof out[r]);
        out[r].index = in[r * cols].index;
        out[r].kind = in[r * cols].kind;
        for (c = 0; c < cols && r * cols + c < n; c++) {
            uint32_t i = r * cols + c;
            cell_put(out[r].text, sizeof out[r].text, &used, in[i].label, cell_chars, (int32_t)i == cursor);
        }
        snprintf(out[r].label, sizeof out[r].label, "%s", in[r * cols].label);
        rows++;
    }
    return rows;
}

int32_t mui_style_row_index(uint32_t style, uint32_t cols, uint32_t packed_row, int32_t cursor) {
    uint32_t kind = mui_style_kind(style);
    if (kind == MUI_STYLE_COVERFLOW) {
        return cursor;
    }
    if (kind == MUI_STYLE_GRID) {
        if (cols == 0u) {
            cols = 1u;
        }
        return cursor >= 0 && (uint32_t)cursor / cols == packed_row ? cursor : (int32_t)(packed_row * cols);
    }
    return (int32_t)packed_row;
}

int32_t mui_style_packed_cursor(uint32_t style, uint32_t cols, int32_t cursor) {
    uint32_t kind = mui_style_kind(style);
    if (cursor < 0 || kind == MUI_STYLE_COVERFLOW) {
        return cursor < 0 ? -1 : 0;
    }
    return kind == MUI_STYLE_GRID ? (int32_t)((uint32_t)cursor / (cols == 0u ? 1u : cols)) : cursor;
}
