# bbmp boyscout replayer (I710c): BoyScout songs (.bsf). The engine is bbmp's csrc/retro/boyscout_engine.cpp (GPL-3.0-or-later), a clean-room
# replayer written after BoyScout 1.0's GBA library (free to use), built unchanged with Selecta's glue (third_party/bbmp_boyscout/
# bboyscout_glue.cpp). GPL, so its own loadable module under SELECTA_GPL_PLUGINS. Host only: C++17 engine (exemption in row I710c).
enable_language(CXX)
set(BBBS_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_boyscout")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_boyscout 1 MODULE adapter.c "${BBBS_DIR}/bboyscout_glue.cpp" THIRD_PARTY_INCLUDES "${BBBS_DIR}")
set_source_files_properties("${BBBS_DIR}/bboyscout_glue.cpp" PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(sel_module_bbmp_boyscout PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_boyscout PROPERTIES CXX_STANDARD 17)
