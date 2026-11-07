# MUI custom class package (design 2026-11-06-mui-mcc-package.md). I685c: the shipped class sources live here and build once, into
# sel_mcc_static (section 3.2: one object list; the Selecta MUI plugin links it, the .mcc libraries of row I685i reuse the same list).
#   src/map/  pure code, built on every target (host tests reach it): aa tokens/geometry, theme store, tooltip plan, icon table
#   src/*.c   draw helpers and tooltip, Amiga only (they include MUI/Intuition headers)
#   src/class/ the ten shipped Selecta_AA* classes, Amiga only
# File names are unchanged in this row; the UP- rename is row I685d.
set(MCC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MCC_MAP_SOURCES src/map/mui_aa.c src/map/mui_aatip.c src/map/mui_theme.c src/map/mui_icons.c src/map/mui_fontprobe.c src/map/mui_log.c)
set(MCC_AMIGA_SOURCES src/mui_aa_draw.c src/mui_aatip.c
  src/class/mui_aabtn_class.c src/class/mui_aatext_class.c src/class/mui_aapanel_class.c src/class/mui_aatabs_class.c
  src/class/mui_aabalance_class.c src/class/mui_aaslider_class.c src/class/mui_aacycle_class.c src/class/mui_aalist_class.c
  src/class/mui_aascroller_class.c src/class/mui_aafield_class.c)
set(MCC_SOURCES ${MCC_MAP_SOURCES})
if(SEL_AMIGA)
  list(APPEND MCC_SOURCES ${MCC_AMIGA_SOURCES})
endif()
# I651/I685e4: button icons (design section 6, D4: the package ships the baked Lucide set). The MUI plugin archive no longer holds them.
# I651: The desktop UI's Lucide set is rasterised at BUILD time by tools/gui_iconbake.c (--mui writes the
# MUI Bodychunk bodies). The tool is compiled with the host compiler here so the Amiga cross build needs no prebuilt path.
find_program(SEL_HOST_CC NAMES cc gcc clang REQUIRED)
set(MCC_ICONBAKE_EXE "${CMAKE_CURRENT_BINARY_DIR}/mcc_iconbake_host")
file(GLOB MCC_LUCIDE_SVGS "${PROJECT_SOURCE_DIR}/third_party/lucide/icons/*.svg")
add_custom_command(
  OUTPUT "${MCC_ICONBAKE_EXE}"
  COMMAND ${SEL_HOST_CC} -O1 -I "${PROJECT_SOURCE_DIR}/include" -o "${MCC_ICONBAKE_EXE}" "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c"
    "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c" -lm
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c" "${PROJECT_SOURCE_DIR}/src/gui/widgets/gui_icon_pack.c"
  COMMENT "Building the host icon baker for the MCC package")
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mui_icons_baked.c"
  COMMAND "${MCC_ICONBAKE_EXE}" "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h" "${PROJECT_SOURCE_DIR}/third_party/lucide/icons"
    "${CMAKE_CURRENT_BINARY_DIR}/mcc_iconbake_unused.c" 24 --mui "${CMAKE_CURRENT_BINARY_DIR}/mui_icons_baked.c"
  DEPENDS "${MCC_ICONBAKE_EXE}" "${PROJECT_SOURCE_DIR}/include/selecta/gui/gui_icons.h" ${MCC_LUCIDE_SVGS}
  COMMENT "Baking the MUI icon bodies")
add_library(sel_mcc_static STATIC ${MCC_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/mui_icons_baked.c")
# src/map: "mui_aa.h" etc. for every consumer; src: "map/mui_theme.h" and "mui_aa_draw.h" spellings the sources already use.
target_include_directories(sel_mcc_static PUBLIC "${MCC_DIR}/include" "${MCC_DIR}/src" "${MCC_DIR}/src/map" "${MCC_DIR}/src/class")
target_include_directories(sel_mcc_static PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/ui/mui")
target_link_libraries(sel_mcc_static PUBLIC selecta_uimodel selecta_gui)
if(SEL_AMIGA)
  target_link_libraries(sel_mcc_static PUBLIC mui)
endif()

# Include roots of the check: the real target's INCLUDE_DIRECTORIES. Shared with the ctest entry in tests/CMakeLists.txt.
set(MCC_CHECK_ROOTS "$<JOIN:$<TARGET_PROPERTY:sel_mcc_static,INCLUDE_DIRECTORIES>,|>" CACHE INTERNAL "")
# Include check: no package file resolves a quoted include outside plugins/shared/mcc.
add_custom_target(mcc-include-check
  COMMAND "${CMAKE_COMMAND}" "-DMCC_DIR=${MCC_DIR}" "-DUI_DIR=${PROJECT_SOURCE_DIR}/plugins/ui/mui"
    "-DSDK_DIR=${PROJECT_SOURCE_DIR}/include/selecta"
    "-DROOTS=${MCC_CHECK_ROOTS}"
    -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_includes.cmake"
  VERBATIM COMMENT "Include check of the MUI class package")
add_dependencies(sdk-isolation mcc-include-check)

if(NOT SEL_AMIGA)
  return()
endif()

# I685b spike: a real MUI external class library (UP-Spike.mcc) and a program that opens it by name. Added from
# plugins/shared/CMakeLists.txt only inside if(SEL_AMIGA), so host builds never see this directory.
# Skeleton: libnix libinit.o (Resident + RTF_AUTOINIT + Open/Close/Expunge/Reserved) + sys-include/stabs.h, no startup code.
set(MCC_LIBINIT_O "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/lib/libinit.o")
set_source_files_properties("${MCC_LIBINIT_O}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
# libinit.o first: the Resident must be the first thing in the hunk, with its -1 returning code ahead of it
add_executable(up_spike_mcc "${MCC_LIBINIT_O}" lib/libheader.c lib/spike_class.c)
set_target_properties(up_spike_mcc PROPERTIES OUTPUT_NAME "UP-Spike.mcc")
target_include_directories(up_spike_mcc PRIVATE "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/include")
target_link_options(up_spike_mcc PRIVATE -nostartfiles)

add_executable(mcc_spike demo/spike.c)
set_target_properties(mcc_spike PROPERTIES OUTPUT_NAME "mcc-spike")
target_link_libraries(mcc_spike PRIVATE mui)
# stabs.h emits the function table with asm() at file scope; -std=c99 hides the asm keyword, so the one file that uses it gets -fasm
set_source_files_properties(lib/libheader.c PROPERTIES COMPILE_OPTIONS "-fasm")
