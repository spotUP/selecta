# bbmp shroom replayer (I710aa): Mario Paint music editor songs (.sho). The engine is bbmp's csrc/retro/shroom_engine.cpp (GPL-3.0-or-later),
# built unchanged with Selecta's glue (third_party/bbmp_shroom/bshroom_glue.cpp). GPL, so its own loadable module under SELECTA_GPL_PLUGINS.
# Host only: C++17 engine (exemption in row I710aa, follow-up I710aaa). The Mario Paint sounds are the user's (system file), never shipped.
enable_language(CXX)
set(BBSH_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_shroom")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_shroom 1 MODULE adapter.c "${BBSH_DIR}/bshroom_glue.cpp" THIRD_PARTY_INCLUDES "${BBSH_DIR}")
set_source_files_properties("${BBSH_DIR}/bshroom_glue.cpp" PROPERTIES COMPILE_OPTIONS "-w;-fvisibility=hidden")
target_link_libraries(sel_module_bbmp_shroom PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_shroom PROPERTIES CXX_STANDARD 17)
