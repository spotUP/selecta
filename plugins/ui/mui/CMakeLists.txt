# Amiga MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P1-1). The mapping functions in map/ are pure C and build everywhere
# so host tests reach them; the plugin entry (mui_ui.c, the only file that will include MUI headers) is registered
# with the shell on the Amiga only, as sel_add_plugin does for text_ui and desktop_ui.
# I651: button icons. The desktop UI's Lucide set is rasterised at BUILD time by tools/gui_iconbake.c (--mui writes the
# MUI Bodychunk bodies). The tool is compiled with the host compiler here so the Amiga cross build needs no prebuilt path.
find_program(SEL_HOST_CC NAMES cc gcc clang REQUIRED)
set(MUI_ICONBAKE_EXE "${CMAKE_CURRENT_BINARY_DIR}/mui_iconbake_host")
file(GLOB MUI_LUCIDE_SVGS "${PROJECT_SOURCE_DIR}/third_party/lucide/icons/*.svg")
add_custom_command(
  OUTPUT "${MUI_ICONBAKE_EXE}"
  COMMAND ${SEL_HOST_CC} -O1 -I "${PROJECT_SOURCE_DIR}/include" -o "${MUI_ICONBAKE_EXE}" "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c"
    "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c" -lm
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c" "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c"
  COMMENT "Building the host icon baker for MUI")
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mui_icons_baked.c"
  COMMAND "${MUI_ICONBAKE_EXE}" "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h" "${PROJECT_SOURCE_DIR}/third_party/lucide/icons"
    "${CMAKE_CURRENT_BINARY_DIR}/mui_iconbake_unused.c" 24 --mui "${CMAKE_CURRENT_BINARY_DIR}/mui_icons_baked.c"
  DEPENDS "${MUI_ICONBAKE_EXE}" "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h" ${MUI_LUCIDE_SVGS}
  COMMENT "Baking the MUI icon bodies")
add_library(mui_map STATIC map/mui_style.c map/mui_wait.c map/mui_probe.c map/mui_menu.c map/mui_layout.c map/mui_controls.c map/mui_list.c map/mui_views.c map/mui_canvas.c map/mui_viz.c map/mui_copper.c map/mui_drop.c map/mui_p8.c map/mui_theme.c map/mui_pens.c map/mui_aa.c map/mui_icons.c map/mui_screen.c "${CMAKE_CURRENT_BINARY_DIR}/mui_icons_baked.c")
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
  target_sources(sel_plugin_mui_ui PRIVATE mui_canvas_class.c mui_listview_class.c mui_aabtn_class.c)
  target_include_directories(sel_plugin_mui_ui PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared") # amiga/p96_write.h
endif()
