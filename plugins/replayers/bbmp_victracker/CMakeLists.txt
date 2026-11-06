# bbmp victracker replayer (I710ai): VIC-TRACKER songs (.vt, Commodore VIC-20) through bbmp's csrc/retro/victracker_engine.cpp (GPL-3.0-or-later),
# which runs VIC-TRACKER 2.0's own BSD player (vt_player.h, assembled once with dasm: third_party/bbmp_victracker/gen_vt_player.sh) on a 6502 with a VIC sound
# generator. GPL, so its own loadable module under SELECTA_GPL_PLUGINS. Host only: C++17 engine (exemption in row I710ai).
enable_language(CXX)
set(BBVT_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_victracker")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_victracker 1 MODULE adapter.c shim.cpp "${BBVT_DIR}/victracker_engine.cpp" THIRD_PARTY_INCLUDES "${BBVT_DIR}")
set_source_files_properties("${BBVT_DIR}/victracker_engine.cpp" shim.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(sel_module_bbmp_victracker PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_victracker PROPERTIES CXX_STANDARD 17)
