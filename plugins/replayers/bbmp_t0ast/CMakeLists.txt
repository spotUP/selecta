# bbmp_t0ast (I710ag): T0AST, "The 0ok Amazing Synth Tracker" (.t0ast), bbmp's csrc/t0ast clean-room engine (GPL-3.0-or-later), unchanged, with Selecta's glue
# (third_party/bbmp_t0ast/bt0ast_glue.cpp). GPL, so its own loadable module under SELECTA_GPL_PLUGINS. Host only (C++17 engine, x87 long double: exemption in row I710ag).
enable_language(CXX)
set(BBT0_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_t0ast")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_t0ast 1 MODULE adapter.c "${BBT0_DIR}/bt0ast_glue.cpp" THIRD_PARTY_INCLUDES "${BBT0_DIR}")
set_source_files_properties("${BBT0_DIR}/bt0ast_glue.cpp" PROPERTIES COMPILE_OPTIONS "-w;-fvisibility=hidden")
target_link_libraries(sel_module_bbmp_t0ast PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_t0ast PROPERTIES CXX_STANDARD 17)
