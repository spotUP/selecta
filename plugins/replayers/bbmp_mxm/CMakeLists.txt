# bbmp mxm replayer (I710f): Cubic Tiny XM (.mxm). The engine is bbmp's csrc/pc/mxm_engine.cpp (GPL-3.0-or-later), a clean-room converter
# to XM, plus bbmp's openmpt.cpp wrapper that hands the XM to libopenmpt (selecta_openmpt, BSD 3-clause, linked statically). Built
# unchanged with -ffp-contract=off as bbmp does. GPL, so its own loadable module under SELECTA_GPL_PLUGINS; the MIT core never links it.
# Host only: C++17 engine and libopenmpt have no 68k build (exemption in row I710f).
enable_language(CXX)
set(BBMXM_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_mxm")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_mxm 1 MODULE adapter.c "${BBMXM_DIR}/mxm_engine.cpp" "${BBMXM_DIR}/openmpt.cpp"
  THIRD_PARTY_INCLUDES "${BBMXM_DIR}" "${SELECTA_OPENMPT_DIR}")
set_source_files_properties("${BBMXM_DIR}/mxm_engine.cpp" "${BBMXM_DIR}/openmpt.cpp" PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_mxm PRIVATE selecta_openmpt ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_mxm PROPERTIES CXX_STANDARD 17)
