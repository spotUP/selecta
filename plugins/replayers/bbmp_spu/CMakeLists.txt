# bbmp spu replayer (I710y): PlayStation SPU logs (.spu). The engine is bbmp's csrc/console/spu_engine.cpp (GPL-2.0-or-later) on the
# P.E.Op.S. SPU of the Audio Overload SDK (third_party/bbmp_spu/peops, GPL-2.0-or-later, built as C: spu.c includes its siblings).
# GPL, so its own loadable module under SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine, no Amiga
# module path (exemption in row I710y).
enable_language(CXX)
set(BBSPU_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_spu")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_spu 1 MODULE adapter.c shim.cpp "${BBSPU_DIR}/peops/spu.c" THIRD_PARTY_INCLUDES "${BBSPU_DIR}")
set_source_files_properties("${BBSPU_DIR}/peops/spu.c" PROPERTIES COMPILE_OPTIONS "-w;-DLSB_FIRST=1")
target_compile_options(sel_module_bbmp_spu PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_spu PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_spu PROPERTIES CXX_STANDARD 17)
