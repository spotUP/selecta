# bbmp jxs replayer (I710i): JayTrax songs (.jxs). The engine is bbmp's csrc/synth/jxs_engine.cpp (GPL-3.0-or-later), a clean-room
# replayer, built unchanged with -fwrapv and -ffp-contract=off as its header asks. GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine, no Amiga module path (exemption in row I710i).
enable_language(CXX)
set(BBJXS_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_jxs")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_jxs 1 MODULE adapter.c "${BBJXS_DIR}/jxs_engine.cpp" THIRD_PARTY_INCLUDES "${BBJXS_DIR}")
set_source_files_properties("${BBJXS_DIR}/jxs_engine.cpp" PROPERTIES COMPILE_OPTIONS "-w;-fwrapv;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_jxs PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_jxs PROPERTIES CXX_STANDARD 17)
