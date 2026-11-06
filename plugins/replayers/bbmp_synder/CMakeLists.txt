# bbmp Synder replayer (I710an): Synder SNG-Player and SNG-Player Stereo songs (.sng) through bbmp's csrc/synder/bbmp-synder.cpp (GPL-3.0-or-later, clean room).
# GPL, so its own loadable module under SELECTA_GPL_PLUGINS. Host only: C++17 engine (exemption in row I710an).
enable_language(CXX)
set(BBSYN_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_synder")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
# -ffp-contract=off: bbmp checks the synthesizer sample for sample against the original program; no fused multiply-add.
sel_add_plugin(bbmp_synder 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${BBSYN_DIR}")
set_source_files_properties(shim.cpp PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_synder PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_synder PROPERTIES CXX_STANDARD 17)
