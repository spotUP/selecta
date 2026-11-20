/* I1473g1: the one cover-grid geometry, shared by the desktop grid and the MUI cover grid (design
 * thoughts/shared/designs/2026-11-20-i1473g-mui-cover-grid.md section 2). Pure C99, no allocation. */
#ifndef SELECTA_UIMODEL_GRID_H
#define SELECTA_UIMODEL_GRID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tiles per row: `avail` is the usable width (no trailing gap), never less than 1 column. */
uint32_t sel_uim_grid_cols(int32_t avail, int32_t tile_w, int32_t gap);

/* First tile row drawn: keeps the cursor's row on screen with `visible_rows` rows showing. */
uint32_t sel_uim_grid_first_row(uint32_t cursor, uint32_t cols, int32_t visible_rows);

/* Point (x, y) to the index into the view window (0 .. row_n-1), or -1 for a gap, the margin or a tile past the window.
 * Tile (0,0) of the first drawn row sits at (x0, y0); window_start is the absolute index of window row 0. */
int32_t sel_uim_grid_hit(int32_t x, int32_t y, int32_t x0, int32_t y0, int32_t tile_w, int32_t tile_h, int32_t gap, uint32_t cols,
                         uint32_t first_row, int32_t visible_rows, uint32_t window_start, uint32_t row_n);

#ifdef __cplusplus
}
#endif

#endif
