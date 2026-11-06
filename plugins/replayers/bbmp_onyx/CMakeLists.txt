# bbmp onyx replayer (I710r): Onyx Music Files (.omf). The engine is bbmp's csrc/retro/jangle_engine.cpp (GPL-3.0-or-later), a clean-room
# converter to a 31-sample module that the vendored libopenmpt (selecta_openmpt, BSD 3-clause, linked statically) plays. GPL, so its own
# loadable module under SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine and libopenmpt have no 68k build
# (exemption in row I710r).
enable_language(CXX)
set(BBONYX_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_onyx")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_onyx 1 MODULE adapter.c shim.cpp "${BBONYX_DIR}/jangle_engine.cpp"
  THIRD_PARTY_INCLUDES "${BBONYX_DIR}" "${SELECTA_OPENMPT_DIR}")
set_source_files_properties("${BBONYX_DIR}/jangle_engine.cpp" shim.cpp PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_onyx PRIVATE selecta_openmpt ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_onyx PROPERTIES CXX_STANDARD 17)
