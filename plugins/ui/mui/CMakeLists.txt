# Amiga MUI UI (plan 2026-10-25-amiga-mui.md, MUI-P1-1). The mapping functions in map/ are pure C and build everywhere
# so host tests reach them; the plugin entry (mui_ui.c, the only file that will include MUI headers) is registered
# with the shell on the Amiga only, as sel_add_plugin does for text_ui and desktop_ui.
add_library(mui_map STATIC map/mui_style.c map/mui_wait.c map/mui_probe.c map/mui_menu.c map/mui_layout.c map/mui_controls.c map/mui_list.c map/mui_views.c map/mui_sidebar.c map/mui_canvas.c map/mui_viz.c map/mui_copper.c map/mui_drop.c map/mui_p8.c map/mui_sources.c map/mui_pens.c map/mui_screen.c map/mui_route.c map/mui_tiles.c map/mui_overlay.c map/mui_aatagmap.c map/mui_iconmap.c map/mui_text.c)
# I685c/I685e4: aa, aatip, theme and icons (with the baked Lucide table) live in plugins/shared/mcc (sel_mcc_static); the
# package references no mui_map symbol, so only this side links the other.
target_link_libraries(mui_map PUBLIC selecta_uimodel selecta_gui sel_mcc_static selecta_shared_source)
target_include_directories(mui_map PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/map" PRIVATE "${PROJECT_SOURCE_DIR}/include")

# Registered on every target: a missing MUI keeps the descriptor (present flag 0) so --ui mui reaches mui_open and its
# failure text (MUI-P1-4, MUI-P1-6). The host build has no MUI calls and always reports the library as absent.
sel_add_plugin(mui_ui 5 mui_ui.c)
target_include_directories(sel_plugin_mui_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/map")
target_link_libraries(sel_plugin_mui_ui PUBLIC mui_map)
# I1305c: the sdk-isolation compile of mui_ui.c clears every include directory, but mui_map.h includes "mui_aa.h" (a class package
# header under plugins/shared, an allowed root); give that compile the package roots sel_mcc_static exports (src/map, src, include).
target_include_directories(sel_sdkcheck_mui_ui PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared/mcc/include" "${PROJECT_SOURCE_DIR}/plugins/shared/mcc/src"
  "${PROJECT_SOURCE_DIR}/plugins/shared/mcc/src/map" "${PROJECT_SOURCE_DIR}/plugins/shared/mcc/src/class" "${CMAKE_CURRENT_SOURCE_DIR}/map")
if(SEL_AMIGA)
  # libmui.a (toolchain) holds the out-of-line varargs MUI_NewObject the ApplicationObject/End idiom needs (I508).
  target_link_libraries(sel_plugin_mui_ui PUBLIC mui)
  target_sources(sel_plugin_mui_ui PRIVATE mui_canvas_class.c mui_vizarea_class.c mui_listview_class.c mui_scrollgroup_class.c mui_keys_class.c mui_aawrap.c mui_cover_image.c) # the AA classes: sel_mcc_static via mui_map
  target_include_directories(sel_plugin_mui_ui PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared" "${PROJECT_SOURCE_DIR}/third_party/stb") # amiga/p96_write.h; stb_image for mui_cover_image.c
  # I1399a: the cover kind icons for mui_canvas_set_icons. The desktop plugin's sel_gui_icons_baked is not linkable from this plugin
  # (separate LoadSeg module), so the same host bake tool writes a second, small set (16..40 px) under another symbol name.
  include(${PROJECT_SOURCE_DIR}/cmake/host_bake_tools.cmake)
  if(SELECTA_ICONBAKE_EXE)
    set(MUI_ICONBAKE_CMD "${SELECTA_ICONBAKE_EXE}")
  else()
    sel_host_iconbake(MUI_ICONBAKE_CMD)
  endif()
  file(GLOB MUI_LUCIDE_SVGS "${PROJECT_SOURCE_DIR}/third_party/lucide/icons/*.svg")
  set(MUI_COVER_ICONS_C "${CMAKE_CURRENT_BINARY_DIR}/mui_cover_icons_baked.c")
  add_custom_command(OUTPUT "${MUI_COVER_ICONS_C}"
    COMMAND ${MUI_ICONBAKE_CMD} "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h"
      "${PROJECT_SOURCE_DIR}/third_party/lucide/icons" "${MUI_COVER_ICONS_C}" 40
    DEPENDS ${MUI_ICONBAKE_CMD} "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h" ${MUI_LUCIDE_SVGS}
    COMMENT "Baking the MUI cover icon set")
  set_source_files_properties("${MUI_COVER_ICONS_C}" PROPERTIES COMPILE_DEFINITIONS "sel_gui_icons_baked=mui_cover_icons_baked")
  target_sources(sel_plugin_mui_ui PRIVATE "${MUI_COVER_ICONS_C}")
endif()
