# bbmp vgemm replayer (I710ah): VGM Music Maker songs (.vge) through bbmp's csrc/retro/vgemm_engine.cpp (GPL-3.0-or-later, clean room), which replays
# the song as the editor's VGM export does and builds the VGM in memory. The engine calls five Game Music Emu functions; selecta_gme has no VGM core
# (libvgm owns VGM, I431), so compat/gme/gme.h and gme_compat.cpp answer them with libvgm's VGM player, the static libs of plugins/replayers/libvgm
# (added first in plugins/CMakeLists.txt, same gate). GPL, so its own loadable module under SELECTA_GPL_PLUGINS. Host only (exemption in row I710ah).
enable_language(CXX)
set(BBVGE_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_vgemm")
set(LIBVGM_DIR "${PROJECT_SOURCE_DIR}/third_party/libvgm")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_vgemm 1 MODULE adapter.c shim.cpp gme_compat.cpp "${BBVGE_DIR}/vgemm_engine.cpp"
  THIRD_PARTY_INCLUDES "${BBVGE_DIR}" "${LIBVGM_DIR}")
# The compat gme.h must win over any <gme/gme.h> further down the path.
target_include_directories(sel_module_bbmp_vgemm BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compat")
set_source_files_properties("${BBVGE_DIR}/vgemm_engine.cpp" shim.cpp gme_compat.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(sel_module_bbmp_vgemm PRIVATE vgm-player vgm-emu vgm-utils ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_vgemm PROPERTIES CXX_STANDARD 17)
