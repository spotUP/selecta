# bbmp buzzic replayer (I710e): Buzzic 1 (.buz) and Buzzic 2 (.buz2). The engine is bbmp's csrc/synth/buz_engine.cpp
# (GPL-3.0-or-later), a clean-room replayer, built unchanged with -ffp-contract=off as bbmp does. GPL, so its own loadable module
# under SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine, no Amiga module path (exemption in row I710e).
enable_language(CXX)
set(BBBUZ_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_buzzic")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_buzzic 1 MODULE adapter.c "${BBBUZ_DIR}/buz_engine.cpp" THIRD_PARTY_INCLUDES "${BBBUZ_DIR}")
set_source_files_properties("${BBBUZ_DIR}/buz_engine.cpp" PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_buzzic PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_buzzic PROPERTIES CXX_STANDARD 17)
