# bbmp berotracker replayer (I710b): BeRoTracker songs (.brt). The engine is bbmp's csrc/berotracker/bbmp-berotracker.cpp (GPL-3.0-or-later),
# a clean-room replayer, included unchanged by third_party/bbmp_berotracker/brt_engine.cpp (main renamed, C API added). Built with
# -ffp-contract=off as bbmp's build script does. GPL, so its own loadable module under SELECTA_GPL_PLUGINS; the MIT core never links it.
# Host only: C++17 engine with x87 long double arithmetic, no Amiga module path (exemption in row I710b).
enable_language(CXX)
set(BBBRT_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_berotracker")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_berotracker 1 MODULE adapter.c "${BBBRT_DIR}/brt_engine.cpp" THIRD_PARTY_INCLUDES "${BBBRT_DIR}")
set_source_files_properties("${BBBRT_DIR}/brt_engine.cpp" PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_berotracker PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_berotracker PROPERTIES CXX_STANDARD 17)
