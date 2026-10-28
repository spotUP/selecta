# Amiga MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P1-1). The mapping functions in map/ are pure C and build everywhere
# so host tests reach them; the plugin entry (mui_ui.c, the only file that will include MUI headers) is registered
# with the shell on the Amiga only, as sel_add_plugin does for text_ui and desktop_ui.
add_library(mui_map STATIC map/mui_style.c map/mui_wait.c map/mui_probe.c map/mui_menu.c map/mui_layout.c map/mui_controls.c map/mui_list.c map/mui_views.c map/mui_canvas.c map/mui_viz.c map/mui_copper.c map/mui_drop.c map/mui_p8.c map/mui_theme.c)
target_link_libraries(mui_map PUBLIC selecta_uimodel selecta_gui)
target_include_directories(mui_map PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/map" PRIVATE "${PROJECT_SOURCE_DIR}/include")

# Registered on every target: a missing MUI keeps the descriptor (present flag 0) so --ui mui reaches mui_open and its
# failure text (MUI-P1-4, MUI-P1-6). The host build has no MUI calls and always reports the library as absent.
sel_add_plugin(mui_ui 5 mui_ui.c)
target_include_directories(sel_plugin_mui_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/map")
target_link_libraries(sel_plugin_mui_ui PUBLIC mui_map)
if(SEL_AMIGA)
  # libmui.a (toolchain) holds the out-of-line varargs MUI_NewObject the ApplicationObject/End idiom needs (I508).
  target_link_libraries(sel_plugin_mui_ui PUBLIC mui)
  target_sources(sel_plugin_mui_ui PRIVATE mui_canvas_class.c)
  target_include_directories(sel_plugin_mui_ui PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared") # amiga/p96_write.h
endif()
