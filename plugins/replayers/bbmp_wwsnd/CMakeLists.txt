# bbmp Westwood SND replayer (I710aj): Westwood PC speaker songs and effects (.snd, Eye of the Beholder 1 and 2, Legend of Kyrandia) through bbmp's
# csrc/retro/wwsnd_engine.cpp (GPL-3.0-or-later, written after ScummVM's PC speaker driver). GPL, so its own loadable module under SELECTA_GPL_PLUGINS.
# Host only: C++17 engine (exemption in row I710aj).
enable_language(CXX)
set(BBWW_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_wwsnd")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_wwsnd 1 MODULE adapter.c shim.cpp "${BBWW_DIR}/wwsnd_engine.cpp" THIRD_PARTY_INCLUDES "${BBWW_DIR}")
set_source_files_properties("${BBWW_DIR}/wwsnd_engine.cpp" shim.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(sel_module_bbmp_wwsnd PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_wwsnd PROPERTIES CXX_STANDARD 17)
