# bbmp ptk replayer (I710q): NoiseTrekker, NoiseTrekker 2, ProTrekkr, ProTrekkr 2.0 songs (.ntk, .ptk) through ProTrekkr's own replay routine
# (third_party/bbmp_ptk, BSD 2-clause, from rePlayer 0.22.14, a few patches listed in VERSION.txt). shim.cpp is written for Selecta; bbmp's GPL
# wrapper is not used. Everything linked is BSD or zlib, so the module is NOT behind SELECTA_GPL_PLUGINS. Host only: C++17 engine, no Amiga
# module path (exemption in ledger row I710q, RULES 4a).
enable_language(CXX)
find_package(ZLIB REQUIRED)
set(BBPK_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_ptk")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
set(BBPK_ENGINE ${BBPK_DIR}/editors/patterns_blocks.cpp ${BBPK_DIR}/files/303s.cpp ${BBPK_DIR}/files/files.cpp ${BBPK_DIR}/files/reverbs.cpp
  ${BBPK_DIR}/files/synths.cpp ${BBPK_DIR}/replay/replay.cpp ${BBPK_DIR}/replay/samples_unpack.cpp ${BBPK_DIR}/replay/spline.cpp
  ${BBPK_DIR}/replay/synth.cpp ${BBPK_DIR}/replay/tb_303.cpp ${BBPK_DIR}/replay/wavpack.cpp)
sel_add_plugin(bbmp_ptk 1 MODULE adapter.c shim.cpp ${BBPK_ENGINE}
  THIRD_PARTY_INCLUDES "${BBPK_DIR};${CMAKE_CURRENT_SOURCE_DIR}/ptk_win;${ZLIB_INCLUDE_DIRS}")
target_compile_options(sel_module_bbmp_ptk PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_compile_definitions(sel_module_bbmp_ptk PRIVATE __STAND_ALONE__ __WINAMP__ __REPLAYER__ __MP3_CODEC__ __GSM_CODEC__ __AT3_CODEC__)
set_source_files_properties(${BBPK_DIR}/replay/wavpack.cpp PROPERTIES COMPILE_DEFINITIONS __GCC__)
target_link_libraries(sel_module_bbmp_ptk PRIVATE ZLIB::ZLIB ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_ptk PROPERTIES CXX_STANDARD 17)
