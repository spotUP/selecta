# I1473g1 shared grid geometry

- New include/selecta/uimodel/grid.h + src/uimodel/grid.c: sel_uim_grid_cols(avail, tile_w, gap), sel_uim_grid_first_row(cursor, cols, visible_rows), sel_uim_grid_hit(...) (point -> window index or -1).
- desktop lib_grid_cols and draw_grid_rows (first row) and MUI mui_grid_cols call them. Behaviour unchanged (cols formula equivalent; avail is width without trailing gap).
- mui_grid_tile_rect only reaches the shared code through mui_grid_cols; its x/y origin math (starts at one gap, square tiles) differs from the desktop and was left alone. sel_uim_grid_hit has no caller yet (row 6 input).
- Test uim-grid-geom (tests/uimodel/grid_test.c). Passed: uim-grid-geom, dui-grid-coverflow, mui-grid, grid-unit (build/sdl).
- Unverified: lint target, Amiga build (not run per brief).
