# I1473g2 MUI tile model
- mui_tile gains `artist`; new `mui_grid_tiles(view, out, cap)` and `mui_grid_style_for(screen_w, bg, tile, sel)` in plugins/ui/mui/map/mui_canvas.[ch]. Both Grid and Cover Flow can use the tile model.
- Tiles skip heading rows; kind via sel_cover_kind_of_item (same as desktop row_kind). Edge 96 below 1024 wide, 128 from 1024; gap/pad 8.
- Test mui-grid-tiles (LABELS mui, in mui_canvas_test "tiles"): PASS, with mui-grid, mui-canvas-geom, mui-art-path.
- Untouched: mui_grid_cols, geometry. Existing mui_ui.c literal grid style at line ~2378 not switched (belongs to wiring row 6).
