# The Amiga cross build cannot run a tool it builds, so the desktop UI plugin is built only when a host-built
# gui_iconbake is given (-DSELECTA_ICONBAKE_EXE=<path>); the canonical Amiga configure leaves the plugin out.
option(SEL_DESKTOP_AGA "Amiga: the desktop plugin draws through the AGA c2p backend instead of RTG (I439, M4-P6-1a)" ON)
include(${PROJECT_SOURCE_DIR}/cmake/desktop_bake_check.cmake)
sel_desktop_bake_check()
if(SEL_DESKTOP_SKIP)
  return()
endif()

# Desktop UI plugin (M3d, plan 2026-10-24-desktop-ui.md section 2.1). The plugin entry and the headless `mem`
# backend build everywhere on the desktop and need no SDL; backend_sdl.c (DUI-P2-7) and input_sdl.c (DUI-P4-1) live in
#  selecta_ui_desktop, which only exists with -DSELECTA_UI_DESKTOP=ON.
#
# Text (DUI-P2-3): the Inter fonts are embedded in the plugin as C arrays generated here from third_party/fonts, so
# no font file is read at run time. atlas_build.c turns a font and a pixel size into the integer atlas tables of
# src/gui/text; it is the only code using stb_truetype (float), outside src/gui.
set(FONT_SRC "${PROJECT_SOURCE_DIR}/third_party/fonts")
set(FONT_GEN "")
# I587: the 68k build embeds no TTF: every Inter atlas it draws is baked (the AGA tests assert atlas_builds=0), and the
# run-time stb_truetype fallback in desktop_ui.c is compiled out there (SEL_DESKTOP_NO_TTF), 0.82 MB of image.
set(FONT_EMBED "Inter-Regular:sel_font_inter_regular" "Inter-SemiBold:sel_font_inter_semibold")
if(SEL_AMIGA)
  set(FONT_EMBED "")
endif()
foreach(spec ${FONT_EMBED})
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
# The Amiga cross build cannot run a tool it builds, so it takes a host-built one: -DSELECTA_ICONBAKE_EXE=<path to the
# host gui_iconbake>.
if(SEL_AMIGA)
  set(ICONBAKE_CMD "${SELECTA_ICONBAKE_EXE}")
  set(ICONBAKE_DEP "")
else()
  add_executable(gui_iconbake "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c")
  target_sources(gui_iconbake PRIVATE "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c")
  target_include_directories(gui_iconbake PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(gui_iconbake PRIVATE m)
  set(ICONBAKE_CMD gui_iconbake)
  set(ICONBAKE_DEP gui_iconbake)
endif()
file(GLOB LUCIDE_SVGS "${PROJECT_SOURCE_DIR}/third_party/lucide/icons/*.svg")
# I584: the baked masks are run-length packed. The Amiga UI runs at density 1 (zoom up to 150 percent of 32 px = 48 px,
# resampled from the next baked size above, 60 px), so its bake stops at 60 px; the host SDL window can ask for density 3.
set(ICONBAKE_MAXPX "")
if(SEL_AMIGA)
  set(ICONBAKE_MAXPX 60)
endif()
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gui_icons_baked.c"
  COMMAND ${ICONBAKE_CMD} "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h"
    "${PROJECT_SOURCE_DIR}/third_party/lucide/icons" "${CMAKE_CURRENT_BINARY_DIR}/gui_icons_baked.c" ${ICONBAKE_MAXPX}
  DEPENDS ${ICONBAKE_DEP} ${SELECTA_ICONBAKE_EXE} "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h" ${LUCIDE_SVGS}
  COMMENT "Baking the icon atlas")
list(APPEND FONT_GEN "${CMAKE_CURRENT_BINARY_DIR}/gui_icons_baked.c")

# Baked text atlases (I463): the 68020 tier has no floating point, so tools/gui_fontbake.c runs the atlas builder
# (atlas_build.c, the same code the host runs at run time) at build time and writes the tables desk_font() uses. Same
# pattern as the icons: the Amiga cross build takes a host-built tool, -DSELECTA_FONTBAKE_EXE=<path to the host gui_fontbake>.
if(SEL_AMIGA)
  set(FONTBAKE_CMD "${SELECTA_FONTBAKE_EXE}")
  set(FONTBAKE_DEP "")
else()
  add_executable(gui_fontbake "${PROJECT_SOURCE_DIR}/tools/gui_fontbake.c" atlas_build.c amiga_font.c "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c")
  target_include_directories(gui_fontbake PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/ui/desktop"
    "${PROJECT_SOURCE_DIR}/third_party/stb")
  target_link_libraries(gui_fontbake PRIVATE m selecta_archive)
  set(FONTBAKE_CMD gui_fontbake)
  set(FONTBAKE_DEP gui_fontbake)
endif()
# I526: the Amiga bitmap fonts (family:file), size = tf_YSize, baked at ydiv 2 next to Inter.
set(AMIGA_MINI_DIR "${FONT_SRC}/amiga-mini")
set(AMIGA_MINI_FILES "${AMIGA_MINI_DIR}/mini4/6" "${AMIGA_MINI_DIR}/mini4/8" "${AMIGA_MINI_DIR}/mini6/6" "${AMIGA_MINI_DIR}/mini6/8" "${AMIGA_MINI_DIR}/mini8/8")
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/font_baked.c"
  COMMAND ${FONTBAKE_CMD} "${FONT_SRC}/Inter-Regular.ttf" "${FONT_SRC}/Inter-SemiBold.ttf" "${CMAKE_CURRENT_BINARY_DIR}/font_baked.c"
    "mini4:${AMIGA_MINI_DIR}/mini4/6" "mini4:${AMIGA_MINI_DIR}/mini4/8" "mini6:${AMIGA_MINI_DIR}/mini6/6" "mini6:${AMIGA_MINI_DIR}/mini6/8" "mini8:${AMIGA_MINI_DIR}/mini8/8"
  DEPENDS ${FONTBAKE_DEP} ${SELECTA_FONTBAKE_EXE} "${FONT_SRC}/Inter-Regular.ttf" "${FONT_SRC}/Inter-SemiBold.ttf" ${AMIGA_MINI_FILES}
  COMMENT "Baking the text atlases")
list(APPEND FONT_GEN "${CMAKE_CURRENT_BINARY_DIR}/font_baked.c")
if(NOT SEL_AMIGA)
  # dui-font-baked (DUI-P4-25): the baked tables equal the runtime builder's atlases.
  add_executable(dui_font_baked_unit "${PROJECT_SOURCE_DIR}/tests/dui/font_baked_unit.c" atlas_build.c "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c" ${FONT_GEN})
  target_include_directories(dui_font_baked_unit PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/ui/desktop"
    "${PROJECT_SOURCE_DIR}/third_party/stb")
  target_link_libraries(dui_font_baked_unit PRIVATE m selecta_archive)
  # dui-font-tall (I538): the ydiv 2 atlases have no vertical partial coverage; the ydiv 1 atlases are unchanged.
  add_executable(dui_font_tall_unit "${PROJECT_SOURCE_DIR}/tests/dui/font_tall_unit.c" atlas_build.c "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c" ${FONT_GEN})
  target_include_directories(dui_font_tall_unit PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/ui/desktop"
    "${PROJECT_SOURCE_DIR}/third_party/stb")
  target_link_libraries(dui_font_tall_unit PRIVATE m selecta_archive)
  # dui-font-roles (I526-P4): the role table and the 6-row fallback.
  add_executable(dui_font_roles_unit "${PROJECT_SOURCE_DIR}/tests/dui/font_roles_unit.c" font_roles.c atlas_build.c "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c" stb_impl.c ${FONT_GEN})
  target_include_directories(dui_font_roles_unit PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/ui/desktop"
    "${PROJECT_SOURCE_DIR}/third_party/stb")
  target_link_libraries(dui_font_roles_unit PRIVATE m selecta_archive)
  # dui-amiga-font (I526-P1): the bitmap .font reader against hand-decoded rows.
  add_executable(dui_amiga_font_unit "${PROJECT_SOURCE_DIR}/tests/dui/amiga_font_unit.c" amiga_font.c)
  target_include_directories(dui_amiga_font_unit PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/ui/desktop")
  target_compile_definitions(dui_amiga_font_unit PRIVATE AMIGA_MINI_DIR="${AMIGA_MINI_DIR}")
endif()

# Artwork (DUI-P2-4): art.c turns library blobs into tile pixels, stb_impl.c decodes them, posix/art_thread.c is the
# worker thread (the only file with thread headers). The plugin is desktop only, so there is no Amiga branch.
if(SEL_AMIGA)
  set(ART_THREAD amiga/art_thread.c amiga/backend_rtg.c amiga/input.c amiga/folder_pick.c)
else()
  set(ART_THREAD posix/art_thread.c)
endif()
sel_add_plugin(desktop_ui 5 desktop_ui.c atlas_build.c font_roles.c art.c ${ART_THREAD} stb_impl.c ${FONT_GEN}
  THIRD_PARTY_INCLUDES "${PROJECT_SOURCE_DIR}/third_party/stb" "${PROJECT_SOURCE_DIR}/third_party/miniz")
set_property(GLOBAL APPEND PROPERTY SEL_GUI_ONLY_PLUGINS desktop_ui) # left out of selecta_registry_cli on the Amiga (I583)
if(SEL_AMIGA AND NOT SEL_DESKTOP_AGA)
  target_compile_definitions(sel_plugin_desktop_ui PRIVATE SELECTA_DESKTOP_RTG=1)
endif()
if(SEL_AMIGA)
  target_compile_definitions(sel_plugin_desktop_ui PRIVATE SEL_DESKTOP_NO_TTF=1)
endif()
target_link_libraries(sel_plugin_desktop_ui PUBLIC selecta_uimodel selecta_gui selecta_archive)
if(SEL_AMIGA)
  target_include_directories(sel_plugin_desktop_ui PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared") # amiga/p96_write.h
endif()

# The aga backend (I439, M4-P6-1, thoughts/shared/plans/2026-10-26-aga-c2p-backend.md): an 8-bitplane Intuition
# screen fed by Kalms' public domain c2p (third_party/kalms-c2p). First light is a probe executable; the plugin
# switches to it with SELECTA_DESKTOP_AGA once desktop_ui.c has the selection branch (design doc 2.1).
if(SEL_AMIGA)
  find_program(SEL_VASM vasmm68k_mot HINTS /opt/homebrew/bin /usr/local/bin NO_CMAKE_FIND_ROOT_PATH)
  if(SEL_VASM)
    set(AGA_C2P_DIR "${PROJECT_SOURCE_DIR}/third_party/kalms-c2p")
    set(AGA_C2P_040 "${CMAKE_CURRENT_BINARY_DIR}/c2p1x1_8_c5_040.o")
    add_custom_command(OUTPUT "${AGA_C2P_040}"
      COMMAND "${SEL_VASM}" -quiet -m68040 -Fhunk -I "${AGA_C2P_DIR}" -o "${AGA_C2P_040}"
        "${AGA_C2P_DIR}/normal/c2p1x1_8_c5_040.s"
      DEPENDS "${AGA_C2P_DIR}/normal/c2p1x1_8_c5_040.s" "${AGA_C2P_DIR}/c2p_equates.i"
      COMMENT "Assembling Kalms c2p1x1_8_c5_040")
    set_source_files_properties("${AGA_C2P_040}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    # I574: the 68020/030 routine; without it a 68020 ran the C reference, 16 s per 640x256 frame.
    set(AGA_C2P_030 "${CMAKE_CURRENT_BINARY_DIR}/c2p1x1_8_c5_030.o")
    add_custom_command(OUTPUT "${AGA_C2P_030}"
      COMMAND "${SEL_VASM}" -quiet -m68020 -m68030 -Fhunk -I "${AGA_C2P_DIR}" -o "${AGA_C2P_030}"
        "${AGA_C2P_DIR}/normal/c2p1x1_8_c5_030.s"
      DEPENDS "${AGA_C2P_DIR}/normal/c2p1x1_8_c5_030.s" "${AGA_C2P_DIR}/c2p_equates.i"
      COMMENT "Assembling Kalms c2p1x1_8_c5_030")
    set_source_files_properties("${AGA_C2P_030}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    if(SEL_DESKTOP_AGA)
      target_sources(sel_plugin_desktop_ui PRIVATE amiga/backend_aga.c amiga/aga_planar.c "${AGA_C2P_040}" "${AGA_C2P_030}")
      target_compile_definitions(sel_plugin_desktop_ui PRIVATE SELECTA_DESKTOP_AGA=1)
    endif()
    add_executable(selecta-aga-first-light amiga/aga_first_light.c amiga/backend_aga.c amiga/aga_planar.c
      amiga/input.c amiga/folder_pick.c "${AGA_C2P_040}" "${AGA_C2P_030}")
    target_link_libraries(selecta-aga-first-light PRIVATE selecta_gui selecta_uimodel)
    # Kalms against the C reference on a 68k CPU; run by `vamos -C 68040` (design doc 6, dui-aga-c2p-kalms).
    add_executable(selecta-aga-c2p-kalms "${PROJECT_SOURCE_DIR}/tests/dui/aga_c2p_kalms.c" amiga/aga_planar.c
      "${AGA_C2P_040}")
    target_link_libraries(selecta-aga-c2p-kalms PRIVATE selecta_gui)
  else()
    if(SEL_DESKTOP_AGA)
      message(FATAL_ERROR "SEL_DESKTOP_AGA needs vasmm68k_mot for the Kalms c2p object")
    endif()
    message(WARNING "vasmm68k_mot not found: the AGA first light probe is not built")
  endif()
endif()
if(UNIX AND NOT SEL_AMIGA)
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
