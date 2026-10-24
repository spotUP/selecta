# Desktop UI plugin (M3d, plan 2026-10-24-desktop-ui.md section 2.1). The plugin entry and the headless `mem`
# backend build everywhere on the desktop and need no SDL; backend_sdl.c (DUI-P2-7) and input_sdl.c (DUI-P4-1) live in
#  selecta_ui_desktop, which only exists with -DSELECTA_UI_DESKTOP=ON.
#
# Text (DUI-P2-3): the Inter fonts are embedded in the plugin as C arrays generated here from third_party/fonts, so
# no font file is read at run time. atlas_build.c turns a font and a pixel size into the integer atlas tables of
# src/gui/text; it is the only code using stb_truetype (float), outside src/gui.
set(FONT_SRC "${PROJECT_SOURCE_DIR}/third_party/fonts")
set(FONT_GEN "")
foreach(spec "Inter-Regular:sel_font_inter_regular" "Inter-SemiBold:sel_font_inter_semibold")
  string(REPLACE ":" ";" pair "${spec}")
  list(GET pair 0 ttf)
  list(GET pair 1 sym)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${sym}.c"
    COMMAND "${CMAKE_COMMAND}" "-DIN=${FONT_SRC}/${ttf}.ttf" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/${sym}.c" "-DSYM=${sym}"
      -P "${PROJECT_SOURCE_DIR}/cmake/embed_blob.cmake"
    DEPENDS "${FONT_SRC}/${ttf}.ttf" "${PROJECT_SOURCE_DIR}/cmake/embed_blob.cmake"
    COMMENT "Embedding ${ttf}")
  list(APPEND FONT_GEN "${CMAKE_CURRENT_BINARY_DIR}/${sym}.c")
endforeach()

# Icons (DUI-P3-1): tools/gui_iconbake.c (host tool, floating point allowed outside src/gui) rasterises the Lucide SVGs of
# third_party/lucide listed in include/selecta/gui/gui_icons.h into alpha masks; the plugin links the generated tables.
add_executable(gui_iconbake "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c")
target_link_libraries(gui_iconbake PRIVATE m)
file(GLOB LUCIDE_SVGS "${PROJECT_SOURCE_DIR}/third_party/lucide/icons/*.svg")
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gui_icons_baked.c"
  COMMAND gui_iconbake "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h"
    "${PROJECT_SOURCE_DIR}/third_party/lucide/icons" "${CMAKE_CURRENT_BINARY_DIR}/gui_icons_baked.c"
  DEPENDS gui_iconbake "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h" ${LUCIDE_SVGS}
  COMMENT "Baking the icon atlas")
list(APPEND FONT_GEN "${CMAKE_CURRENT_BINARY_DIR}/gui_icons_baked.c")

# Artwork (DUI-P2-4): art.c turns library blobs into tile pixels, stb_impl.c decodes them, posix/art_thread.c is the
# worker thread (the only file with thread headers). The plugin is desktop only, so there is no Amiga branch.
sel_add_plugin(desktop_ui 5 desktop_ui.c atlas_build.c art.c posix/art_thread.c stb_impl.c ${FONT_GEN}
  THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/stb")
target_link_libraries(sel_plugin_desktop_ui PUBLIC selecta_uimodel selecta_gui)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(sel_plugin_desktop_ui PUBLIC m Threads::Threads)
endif()

# The sdl backend (DUI-P2-7) is a separate library so the plugin's own sources stay SDK-clean; it needs SDL3 and the
# gui headers only. The plugin sees it through SELECTA_DESKTOP_SDL.
if(SELECTA_UI_DESKTOP)
  add_library(selecta_ui_desktop STATIC backend_sdl.c)
  target_include_directories(selecta_ui_desktop PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(selecta_ui_desktop PUBLIC SDL3::SDL3 selecta_gui)
  target_link_libraries(sel_plugin_desktop_ui PUBLIC selecta_ui_desktop)
  target_compile_definitions(sel_plugin_desktop_ui PRIVATE SELECTA_DESKTOP_SDL=1)
endif()
