/* I1473g1: shared cover-grid geometry (include/selecta/uimodel/grid.h). */
#include <selecta/uimodel/grid.h>

uint32_t sel_uim_grid_cols(int32_t avail, int32_t tile_w, int32_t gap) {
    int32_t n = tile_w + gap > 0 ? (avail + gap) / (tile_w + gap) : 1;
    return n > 1 ? (uint32_t)n : 1u;
}

uint32_t sel_uim_grid_first_row(uint32_t cursor, uint32_t cols, int32_t visible_rows) {
    uint32_t cur_row = cols > 0u ? cursor / cols : 0u;
    return visible_rows > 0 && cur_row >= (uint32_t)visible_rows ? cur_row - (uint32_t)visible_rows + 1u : 0u;
}

int32_t sel_uim_grid_hit(int32_t x, int32_t y, int32_t x0, int32_t y0, int32_t tile_w, int32_t tile_h, int32_t gap, uint32_t cols,
                         uint32_t first_row, int32_t visible_rows, uint32_t window_start, uint32_t row_n) {
    int32_t dx = x - x0, dy = y - y0, col, row;
    uint32_t g;
    if (dx < 0 || dy < 0 || cols == 0u || tile_w <= 0 || tile_h <= 0) {
        return -1;
    }
    col = dx / (tile_w + gap);
    row = dy / (tile_h + gap);
    if (dx % (tile_w + gap) >= tile_w || dy % (tile_h + gap) >= tile_h || (uint32_t)col >= cols || row >= visible_rows) {
        return -1;
    }
    g = (first_row + (uint32_t)row) * cols + (uint32_t)col;
    return g >= window_start && g - window_start < row_n ? (int32_t)(g - window_start) : -1;
}
