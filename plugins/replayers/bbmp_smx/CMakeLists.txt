# bbmp SCC-Musixx replayer (I710am): SCC-Musixx songs (.sng, MSX, Konami SCC) through bbmp's csrc/retro/smx_engine.cpp (GPL-3.0-or-later, clean room) on
# emu2212 (MIT). GPL, so its own loadable module under SELECTA_GPL_PLUGINS. Host only: C++17 engine (exemption in row I710am).
enable_language(CXX)
set(BBSMX_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_smx")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
# -ffp-contract=off: the engine's resampling is checked bit for bit by bbmp; no fused multiply-add.
sel_add_plugin(bbmp_smx 1 MODULE adapter.c shim.cpp "${BBSMX_DIR}/smx_engine.cpp" "${BBSMX_DIR}/emu2212.c" THIRD_PARTY_INCLUDES "${BBSMX_DIR}")
set_source_files_properties("${BBSMX_DIR}/smx_engine.cpp" "${BBSMX_DIR}/emu2212.c" shim.cpp PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_smx PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_smx PROPERTIES CXX_STANDARD 17)
