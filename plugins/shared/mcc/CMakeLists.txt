# MUI custom class package (design 2026-11-06-mui-mcc-package.md). I685c: the shipped class sources live here and build once, into
# sel_mcc_static (section 3.2: one object list; the Selecta MUI plugin links it, the .mcc libraries of row I685i reuse the same list).
#   src/map/  pure code, built on every target (host tests reach it): aa tokens/geometry, theme store, tooltip plan, icon table
#   src/*.c   draw helpers and tooltip, Amiga only (they include MUI/Intuition headers)
#   src/class/ the ten shipped Selecta_AA* classes, Amiga only
# File names are unchanged in this row; the UP- rename is row I685d.
set(MCC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(MCC_MAP_SOURCES src/map/mui_aa.c src/map/mui_aatip.c src/map/mui_theme.c src/map/mui_icons.c src/map/mui_fontprobe.c src/map/mui_log.c src/map/up_prefs_map.c src/map/up_origin.c src/map/up_store.c)
set(MCC_AMIGA_SOURCES src/mui_aa_draw.c src/mui_aatip.c
  src/class/mui_aabtn_class.c src/class/mui_aatext_class.c src/class/mui_aapanel_class.c src/class/mui_aatabs_class.c
  src/class/mui_aabalance_class.c src/class/mui_aaslider_class.c src/class/mui_aacycle_class.c src/class/mui_aalist_class.c
  src/class/mui_aascroller_class.c src/class/mui_aafield_class.c)
# I685i: the eleven UP-<Name>.mcc libraries (section 3.2/3.3), one row each: name | class source | description symbol of the file.
# Slider and Progress both come from mui_aaslider_class.c. The class sources of the libraries must be the class sources of
# sel_mcc_static (ctest mcc-classlibs-source; the Amiga part below also checks every library's SOURCES).
set(MCC_CLASS_LIBS
  "Button|src/class/mui_aabtn_class.c|aabtn_class_desc"
  "Text|src/class/mui_aatext_class.c|aatext_class_desc"
  "Panel|src/class/mui_aapanel_class.c|aapanel_class_desc"
  "Tabs|src/class/mui_aatabs_class.c|aatabs_class_desc"
  "Balance|src/class/mui_aabalance_class.c|aabal_class_desc"
  "Slider|src/class/mui_aaslider_class.c|aaslider_class_desc"
  "Progress|src/class/mui_aaslider_class.c|aaprogress_class_desc"
  "Cycle|src/class/mui_aacycle_class.c|aacycle_class_desc"
  "Field|src/class/mui_aafield_class.c|aafield_class_desc"
  "Scroller|src/class/mui_aascroller_class.c|aascroller_class_desc"
  "List|src/class/mui_aalist_class.c|aalist_class_desc")
set(_mcc_lib_sources "")
foreach(row ${MCC_CLASS_LIBS})
  string(REPLACE "|" ";" cols "${row}")
  list(GET cols 1 src)
  list(APPEND _mcc_lib_sources "${src}")
endforeach()
set(_mcc_names "")
foreach(row ${MCC_CLASS_LIBS})
  string(REPLACE "|" ";" cols "${row}")
  list(GET cols 0 n)
  list(APPEND _mcc_names "${n}")
endforeach()
set(MCC_CLASS_NAMES "${_mcc_names}" CACHE INTERNAL "class names of the .mcc libraries (UP-<Name>.mcc)")
set(MCC_CLASS_LIB_SOURCES "${_mcc_lib_sources}" CACHE INTERNAL "class source of each .mcc library")
set(MCC_STATIC_SOURCES "${MCC_AMIGA_SOURCES}" CACHE INTERNAL "sources of sel_mcc_static that only the Amiga build compiles")
set(MCC_SOURCES ${MCC_MAP_SOURCES} lib/support_lib.c)
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

# I685j: file name of the MUI prefs class library; tests/ui/mcc_prefs_test.c checks it against the target below.
set(MCC_PREFS_NAME "UP-MCC.mcp" CACHE STRING "file name of the MUI prefs class library")

# I685k: UP-Demo is built against the release headers only. MCC_DEMO_INCLUDES / MCC_DEMO_LIBS are the whole include and link
# interface of target up_demo (asserted below on the real target); ctest mcc-demo-headers-only checks them and the demo source.
set(MCC_RELEASE_INC "${CMAKE_CURRENT_BINARY_DIR}/release_include" CACHE INTERNAL "staged release headers of the package")
set(MCC_DEMO_INCLUDES "${MCC_RELEASE_INC}" "${CMAKE_CURRENT_BINARY_DIR}/sfd_include" CACHE INTERNAL "include directories of up_demo")
set(MCC_DEMO_LIBS "mui" CACHE INTERNAL "link libraries of up_demo")

if(NOT SEL_AMIGA)
  return()
endif()

# I685b spike: a real MUI external class library (UP-Spike.mcc) and a program that opens it by name. Added from
# plugins/shared/CMakeLists.txt only inside if(SEL_AMIGA), so host builds never see this directory.
# Skeleton: lib/up_libinit.c (Resident + RTF_AUTOINIT + Open/Close/Expunge/Reserved, libnix libinit.o transcribed) +
# sys-include/stabs.h, no startup code. Not libinit.o itself: its Resident has rt_Version 0, and ramlib refuses
# OpenLibrary(name, n) for a disk library whose Resident version is below n (I685p). MCC_LIB_VERSION is the one version
# source: the Resident (UP_LIB_VERSION in up_libinit.c) and LibVersion in each header file. Post-build check: cmake/mcc_check_romtag.cmake.
set(MCC_LIB_VERSION 1)
set(MCC_LIBINIT_SRC lib/up_libinit.c)
set_source_files_properties(${MCC_LIBINIT_SRC} PROPERTIES COMPILE_OPTIONS "-fasm")
function(up_mcc_lib_skeleton tgt)
  target_compile_definitions(${tgt} PRIVATE "UP_LIB_VERSION=${MCC_LIB_VERSION}")
  add_custom_command(TARGET ${tgt} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DBINARY=$<TARGET_FILE:${tgt}>" "-DVERSION=${MCC_LIB_VERSION}"
      -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_romtag.cmake"
    VERBATIM COMMENT "${tgt}: Resident version ${MCC_LIB_VERSION}")
endfunction()
# up_libinit.c first: the Resident must be in the first hunk, with its -1 returning code ahead of it
add_executable(up_spike_mcc ${MCC_LIBINIT_SRC} lib/libheader.c lib/spike_class.c)
set_target_properties(up_spike_mcc PROPERTIES OUTPUT_NAME "UP-Spike.mcc")
target_include_directories(up_spike_mcc PRIVATE "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/include")
target_link_options(up_spike_mcc PRIVATE -nostartfiles)
up_mcc_lib_skeleton(up_spike_mcc)

add_executable(mcc_spike demo/spike.c)
set_target_properties(mcc_spike PROPERTIES OUTPUT_NAME "mcc-spike")
target_link_libraries(mcc_spike PRIVATE mui)
# stabs.h emits the function table with asm() at file scope; -std=c99 hides the asm keyword, so the one file that uses it gets -fasm
set_source_files_properties(lib/libheader.c PROPERTIES COMPILE_OPTIONS "-fasm")

# I685h: up-mcc.library, the support library (theme, icons, font list; design section 8.4). Same skeleton as the spike; the
# functions are lib/support_lib.c from sel_mcc_static (also linked by the Selecta plugin), the table is in lib/support_libheader.c.
add_executable(up_mcc_library ${MCC_LIBINIT_SRC} lib/support_libheader.c)
set_target_properties(up_mcc_library PROPERTIES OUTPUT_NAME "up-mcc.library")
target_include_directories(up_mcc_library PRIVATE "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/include")
target_link_libraries(up_mcc_library PRIVATE sel_mcc_static)
target_link_options(up_mcc_library PRIVATE -nostartfiles)
up_mcc_lib_skeleton(up_mcc_library)
set_source_files_properties(lib/support_libheader.c PROPERTIES COMPILE_OPTIONS "-fasm")

# The SFD file generates the C headers a client includes (proto/up_mcc.h, inline/up_mcc.h, clib/up_mcc_protos.h, pragmas) at build
# time with sfdc. up_mcc_header_check compiles a client against them.
set(MCC_SFDC "$ENV{HOME}/opt/amiga/bin/sfdc" CACHE FILEPATH "sfdc, the SFD to header generator")
set(MCC_SFD "${CMAKE_CURRENT_SOURCE_DIR}/dist/sfd/up-mcc_lib.sfd")
set(MCC_SFD_INC "${CMAKE_CURRENT_BINARY_DIR}/sfd_include")
set(MCC_SFD_HEADERS "${MCC_SFD_INC}/proto/up_mcc.h" "${MCC_SFD_INC}/inline/up_mcc.h" "${MCC_SFD_INC}/clib/up_mcc_protos.h"
  "${MCC_SFD_INC}/pragmas/up_mcc_pragmas.h")
add_custom_command(
  OUTPUT ${MCC_SFD_HEADERS}
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${MCC_SFD_INC}/proto" "${MCC_SFD_INC}/inline" "${MCC_SFD_INC}/clib" "${MCC_SFD_INC}/pragmas"
  COMMAND "${MCC_SFDC}" -q --target=m68k-amigaos --mode=proto "${MCC_SFD}" -o "${MCC_SFD_INC}/proto/up_mcc.h"
  COMMAND "${MCC_SFDC}" -q --target=m68k-amigaos --mode=macros "${MCC_SFD}" -o "${MCC_SFD_INC}/inline/up_mcc.h"
  COMMAND "${MCC_SFDC}" -q --target=m68k-amigaos --mode=clib "${MCC_SFD}" -o "${MCC_SFD_INC}/clib/up_mcc_protos.h"
  COMMAND "${MCC_SFDC}" -q --target=m68k-amigaos --mode=pragmas "${MCC_SFD}" -o "${MCC_SFD_INC}/pragmas/up_mcc_pragmas.h"
  DEPENDS "${MCC_SFD}" COMMENT "sfdc: headers of up-mcc.library")
add_library(up_mcc_header_check OBJECT demo/support_header_check.c ${MCC_SFD_HEADERS})
target_include_directories(up_mcc_header_check PRIVATE "${MCC_SFD_INC}" "${MCC_DIR}/include")

# I685i: one library per class from lib/classheader.c. up_libinit.c first (Resident), -nostartfiles, the class file with
# -DUP_MCC_LIBRARY so its constructors use the class the library init made, everything else from sel_mcc_static (open up-mcc.library
# instead of linking the static support code is a later row). The check at the end compares each target's SOURCES with the list.
# I685n: what every .mcc/.mcp compiles itself, ahead of the archive, so the linker never pulls the archive's token store and registry
# (src/map/up_store.c) or its draw context list: the client of the store (UP_CLASS_LIBRARY) and the draw code that registers with it.
set(MCC_CLIENT_SOURCES src/map/up_store_client.c src/mui_aa_draw.c)
function(up_add_class_library name class_source desc)
  set(tgt "up_mcc_${name}")
  add_executable(${tgt} ${MCC_LIBINIT_SRC} lib/classheader.c "${class_source}" ${MCC_CLIENT_SOURCES} ${MCC_SFD_HEADERS})
  set_target_properties(${tgt} PROPERTIES OUTPUT_NAME "UP-${name}.mcc")
  target_include_directories(${tgt} PRIVATE "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/include" "${MCC_SFD_INC}")
  target_compile_definitions(${tgt} PRIVATE "UP_CLASS_NAME=\"${name}\"" "UP_CLASS_DESC=${desc}" UP_MCC_LIBRARY UP_CLASS_LIBRARY)
  target_link_libraries(${tgt} PRIVATE sel_mcc_static)
  target_link_options(${tgt} PRIVATE -nostartfiles)
  up_mcc_lib_skeleton(${tgt})
  set_source_files_properties(lib/classheader.c PROPERTIES COMPILE_OPTIONS "-fasm")
  get_target_property(srcs ${tgt} SOURCES)
  if(NOT "${class_source}" IN_LIST MCC_STATIC_SOURCES OR NOT "${class_source}" IN_LIST srcs)
    message(FATAL_ERROR "${tgt}: class source ${class_source} is not the one sel_mcc_static compiles")
  endif()
endfunction()
find_program(MCC_NM NAMES m68k-amigaos-nm HINTS "${AMIGA_GCC_ROOT}/bin" "$ENV{HOME}/opt/amiga/bin" REQUIRED)
# I685n: post-build nm check on every .mcc/.mcp: no token store, no registry (cmake/mcc_check_classlibs.cmake, nm mode).
function(up_check_no_store tgt)
  add_custom_command(TARGET ${tgt} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DNM=${MCC_NM}" "-DBINARY=$<TARGET_FILE:${tgt}>" "-DNM_OUT=$<TARGET_FILE:${tgt}>.nm-store.txt"
      -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_classlibs.cmake"
    VERBATIM COMMENT "${tgt}: no theme store symbols")
endfunction()
foreach(row ${MCC_CLASS_LIBS})
  string(REPLACE "|" ";" cols "${row}")
  list(GET cols 0 n)
  list(GET cols 1 s)
  list(GET cols 2 d)
  up_add_class_library(${n} "${s}" ${d})
  up_check_no_store(up_mcc_${n})
endforeach()
# I685j: UP-MCC.mcp from the same skeleton (UP_PREFS_LIBRARY: MCC_Query(1) returns the class).
add_executable(up_mcc_prefs ${MCC_LIBINIT_SRC} lib/classheader.c prefs/up_prefs.c ${MCC_CLIENT_SOURCES} ${MCC_SFD_HEADERS})
set_target_properties(up_mcc_prefs PROPERTIES OUTPUT_NAME "${MCC_PREFS_NAME}")
target_include_directories(up_mcc_prefs PRIVATE "${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/include" "${MCC_SFD_INC}")
target_compile_definitions(up_mcc_prefs PRIVATE "UP_CLASS_NAME=\"MCC\"" UP_CLASS_DESC=up_prefs_class_desc UP_MCC_LIBRARY UP_PREFS_LIBRARY UP_CLASS_LIBRARY)
target_link_libraries(up_mcc_prefs PRIVATE sel_mcc_static)
target_link_options(up_mcc_prefs PRIVATE -nostartfiles)
up_mcc_lib_skeleton(up_mcc_prefs)
up_check_no_store(up_mcc_prefs)
add_custom_target(up_mcc_classes DEPENDS up_mcc_Button up_mcc_Text up_mcc_Panel up_mcc_Tabs up_mcc_Balance up_mcc_Slider up_mcc_Progress
  up_mcc_Cycle up_mcc_Field up_mcc_Scroller up_mcc_List up_mcc_prefs)

# I685k: UP-Demo (demo/updemo.c). Headers: the release set staged here (include/mcc/*.h plus the one header up.h needs,
# selecta/uimodel/tokens.h, which the release archive carries as Include/selecta/uimodel/tokens.h) and the sfdc headers. No Selecta
# include path and no link against sel_mcc_static.
configure_file("${MCC_DIR}/include/mcc/up.h" "${MCC_RELEASE_INC}/mcc/up.h" COPYONLY)
configure_file("${MCC_DIR}/include/mcc/up_lib.h" "${MCC_RELEASE_INC}/mcc/up_lib.h" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/include/selecta/uimodel/tokens.h" "${MCC_RELEASE_INC}/selecta/uimodel/tokens.h" COPYONLY)
add_executable(up_demo demo/updemo.c ${MCC_SFD_HEADERS})
set_target_properties(up_demo PROPERTIES OUTPUT_NAME "UP-Demo")
target_include_directories(up_demo PRIVATE ${MCC_DEMO_INCLUDES})
target_link_libraries(up_demo PRIVATE ${MCC_DEMO_LIBS})
get_target_property(_demo_inc up_demo INCLUDE_DIRECTORIES)
get_target_property(_demo_libs up_demo LINK_LIBRARIES)
if(NOT "${_demo_inc}" STREQUAL "${MCC_DEMO_INCLUDES}" OR NOT "${_demo_libs}" STREQUAL "${MCC_DEMO_LIBS}")
  message(FATAL_ERROR "up_demo: include/link interface differs from MCC_DEMO_INCLUDES/MCC_DEMO_LIBS (${_demo_inc} | ${_demo_libs})")
endif()
# Gate item 1 symbol check, part of the build: nm on the finished UP-Demo; the count goes to UP-Demo.nm-count.txt.
add_custom_command(TARGET up_demo POST_BUILD
  COMMAND "${CMAKE_COMMAND}" "-DNM=${MCC_NM}" "-DBINARY=$<TARGET_FILE:up_demo>" "-DCOUNT_FILE=$<TARGET_FILE:up_demo>.nm-count.txt"
    -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_demo_symbols.cmake"
  VERBATIM COMMENT "UP-Demo: no class code symbols")

# I685l: the distribution tree (design section 9). cmake/mcc_dist.cmake stages UP-MCC/ from the build products and the sources, checks
# the layout, and when lha is found packs UP-MCC.lha next to it. No lha: the staged tree stays and the CI step zips it.
set(MCC_DIST_DIR "${CMAKE_CURRENT_BINARY_DIR}/dist")
find_program(MCC_LHA NAMES lha)
add_custom_target(up_mcc_dist
  COMMAND "${CMAKE_COMMAND}" -DMODE=stage "-DDEST=${MCC_DIST_DIR}/UP-MCC" "-DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}" "-DSFD_INC=${MCC_SFD_INC}"
    "-DROOT=${PROJECT_SOURCE_DIR}" "-DCLASS_NAMES=${MCC_CLASS_NAMES}" -P "${PROJECT_SOURCE_DIR}/cmake/mcc_dist.cmake"
  COMMAND "${CMAKE_COMMAND}" -E copy "${MCC_DIR}/dist/UP-MCC.readme" "${MCC_DIST_DIR}/UP-MCC.readme"
  VERBATIM COMMENT "Staging UP-MCC (section 9)")
add_dependencies(up_mcc_dist up_mcc_library up_mcc_classes up_demo up_mcc_header_check)
if(MCC_LHA)
  add_custom_command(TARGET up_mcc_dist POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E remove -f "${MCC_DIST_DIR}/UP-MCC.lha"
    COMMAND "${MCC_LHA}" ao5 "${MCC_DIST_DIR}/UP-MCC.lha" UP-MCC
    WORKING_DIRECTORY "${MCC_DIST_DIR}" VERBATIM COMMENT "Packing UP-MCC.lha")
else()
  message(STATUS "up_mcc_dist: lha not found, the staged tree is not packed (CI zips it)")
endif()
