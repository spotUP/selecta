# bbmp axs replayer (I710a): AXS songs (.axs). The engine is bbmp's csrc/axs/bbmp-axs.cpp (GPL-3.0-or-later), a clean-room replayer,
# built unchanged (main() compiled out by its own AXS_NO_MAIN switch, in third_party/bbmp_axs/axs_engine.cpp). GPL, so its own
# loadable module under SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine, no Amiga module path (exemption in row I710a).
enable_language(CXX)
set(BBAXS_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_axs")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_axs 1 MODULE adapter.c "${BBAXS_DIR}/axs_engine.cpp" THIRD_PARTY_INCLUDES "${BBAXS_DIR}")
set_source_files_properties("${BBAXS_DIR}/axs_engine.cpp" PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(sel_module_bbmp_axs PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_axs PROPERTIES CXX_STANDARD 17)
