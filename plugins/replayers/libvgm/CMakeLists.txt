# libvgm replayer (I431): libvgm (ValleyBell, pinned in third_party/libvgm/REVISION), the engine bbmp uses for vgm.
# Several sound cores are GPL-2.0-or-later (YM2413, YM2151, OPL, NES APU), so the module is GPL: it only ever builds
# as a loadable module, and only when SELECTA_GPL_PLUGINS=ON; the core binaries never link it. See LICENSE.
enable_language(CXX)
set(LIBVGM_DIR "${PROJECT_SOURCE_DIR}/third_party/libvgm")

set(BUILD_LIBAUDIO OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(BUILD_VGM2WAV OFF CACHE BOOL "" FORCE)
set(USE_SANITIZERS OFF CACHE BOOL "" FORCE)
set(LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory("${LIBVGM_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libvgm" EXCLUDE_FROM_ALL)

# Upstream code is not clean under the project's -Wall -Wextra -Werror -pedantic; third-party, so warnings are off
# (same as sid and zxtune).
foreach(lib vgm-utils vgm-emu vgm-player)
  target_compile_options(${lib} PRIVATE -w -fvisibility=hidden)
endforeach()

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(libvgm 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${LIBVGM_DIR}")
target_include_directories(sel_module_libvgm PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sel_module_libvgm PRIVATE vgm-player vgm-emu vgm-utils ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_libvgm PROPERTIES CXX_STANDARD 14)
