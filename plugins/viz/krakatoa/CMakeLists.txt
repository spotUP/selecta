# Krakatoa Smoke (I1052, design thoughts/shared/research/2026-10-27-krakatoa-port-design.md section 2, modern tier).
# Three parts:
#   krk_core            portable C: quality and grid, knobs, palettes, audio mapper, autopilot, uniform packing
#   selecta_krk_gpu     the SDL3 GPU compute host (SDL headers stay out of the plugin sources, as with backend_sdl.c)
#   sel_plugin_viz_krakatoa  the viz plugin; only its own directory, include/selecta and plugins/shared are included
# Shaders: shaders/krk_smoke.comp -> SPIR-V (glslangValidator) -> MSL 2.1 (spirv-cross) -> krk_shaders_msl.h at build
# time. Only MSL ships (APPLE); the Vulkan / SPIR-V binding layout for Linux is open as KRK-P4-5.
find_program(KRK_GLSLANG glslangValidator)
find_program(KRK_SPVCROSS spirv-cross)
if(NOT KRK_GLSLANG OR NOT KRK_SPVCROSS)
  message(STATUS "viz_krakatoa: glslangValidator or spirv-cross not found, Krakatoa not built")
  set(SEL_HAVE_KRAKATOA OFF CACHE INTERNAL "viz_krakatoa is built")
  return()
endif()
set(SEL_HAVE_KRAKATOA ON CACHE INTERNAL "viz_krakatoa is built")

add_library(krk_core STATIC core/krk_core.c)
target_include_directories(krk_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
if(UNIX)
  target_link_libraries(krk_core PUBLIC m)
endif()

set(krk_gen "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(krk_src "${CMAKE_CURRENT_SOURCE_DIR}/shaders/krk_smoke.comp")
add_custom_command(
  OUTPUT "${krk_gen}/krk_shaders_msl.h"
  COMMAND "${CMAKE_COMMAND}" "-DGLSLANG=${KRK_GLSLANG}" "-DSPVCROSS=${KRK_SPVCROSS}" "-DSRC=${krk_src}"
    "-DWORK=${krk_gen}/work" "-DOUT=${krk_gen}/krk_shaders_msl.h" -P "${PROJECT_SOURCE_DIR}/cmake/krk_embed.cmake"
  DEPENDS "${krk_src}" "${PROJECT_SOURCE_DIR}/cmake/krk_embed.cmake"
  COMMENT "Krakatoa shaders to MSL" VERBATIM)

add_library(selecta_krk_gpu STATIC sdl/krk_smoke_gpu.c "${krk_gen}/krk_shaders_msl.h")
target_include_directories(selecta_krk_gpu PRIVATE "${krk_gen}")
target_link_libraries(selecta_krk_gpu PUBLIC SDL3::SDL3 krk_core)

sel_add_plugin(viz_krakatoa 4 krakatoa.c)
target_link_libraries(sel_plugin_viz_krakatoa PUBLIC selecta_krk_gpu)
