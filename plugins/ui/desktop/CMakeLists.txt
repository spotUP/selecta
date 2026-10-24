# Desktop UI plugin (M3d, plan 2026-10-24-desktop-ui.md section 2.1). The plugin entry and the headless `mem`
# backend build everywhere on the desktop and need no SDL; backend_sdl.c and input_sdl.c arrive with DUI-P2-7 and
# DUI-P4-1 inside selecta_ui_desktop, which only exists with -DSELECTA_UI_DESKTOP=ON.
sel_add_plugin(desktop_ui 5 desktop_ui.c)
target_link_libraries(sel_plugin_desktop_ui PUBLIC selecta_uimodel)

if(SELECTA_UI_DESKTOP)
  add_library(selecta_ui_desktop STATIC deps_probe.c)
  target_link_libraries(selecta_ui_desktop PUBLIC SDL3::SDL3)
endif()
