# I1473m: MUI rounded covers

Design twin of e3c0ec9d (desktop: generated and grid covers rounded, Now Playing full-bleed cover stays square).

Changed: plugins/ui/mui/map/mui_canvas.c, mui_canvas_draw_grid only.
- Real covers in the grid: sel_gui_blit_image_round (shared C, same as the Mac).
- Generated platform covers: mui_platform_cover, then mui_round_corners repaints the outside-arc corner pixels with the tile colour (integer test, no alpha, corners only).
- Radius: mui_cover_radius = Mac cover_radius rule (RADIUS_LARGE from 96 px, else RADIUS), from sel_gui_token_default (no theme in the MUI class).
- Left square on purpose: the single Now Playing cover (mui_canvas_draw_art path, and the AGA RectFill / cube image path), like the Mac.

Unverified (no build allowed):
- Does not compile-checked; sel_gui_token_default signature assumed (u32 return, token enum arg).
- tests/ui/mui_canvas_test.c MUI_GRID_GOLDEN hash will change and needs a rebless after a build.
- mui_canvas_set_grid has no caller today (per the test comment), so no live MUI screen draws the grid yet; nothing visible on a rig until it is wired.
- Visual look of the corner arc at 32-64 px edges.
