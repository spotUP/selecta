# Desktop UI plugin (M3d, plan 2026-10-24-desktop-ui.md section 2.1). Phase 0 only proves the SDL3 dependency
# builds; the descriptor, backend_sdl.c and input_sdl.c arrive with DUI-P2-7 and DUI-P4-1.
add_library(selecta_ui_desktop STATIC deps_probe.c)
target_link_libraries(selecta_ui_desktop PUBLIC SDL3::SDL3)
