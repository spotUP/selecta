# bbmp ptk replayer (I710q): NoiseTrekker, NoiseTrekker 2, ProTrekkr, ProTrekkr 2.0 songs (.ntk, .ptk) through ProTrekkr's own replay routine
# (third_party/bbmp_ptk, BSD 2-clause, from rePlayer 0.22.14, a few patches listed in VERSION.txt). shim.cpp is written for Selecta; bbmp's GPL
# wrapper is not used. Everything linked is BSD or zlib, so the module is NOT behind SELECTA_GPL_PLUGINS. A loadable module on the host, a static
# plugin on the Amiga (I710qa): there the zlib calls go to the vendored miniz through ptk_zlib/zlib.h, the toolchain has no libz.
enable_language(CXX)
set(BBPK_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_ptk")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
set(BBPK_ENGINE ${BBPK_DIR}/editors/patterns_blocks.cpp ${BBPK_DIR}/files/303s.cpp ${BBPK_DIR}/files/files.cpp ${BBPK_DIR}/files/reverbs.cpp
  ${BBPK_DIR}/files/synths.cpp ${BBPK_DIR}/replay/replay.cpp ${BBPK_DIR}/replay/samples_unpack.cpp ${BBPK_DIR}/replay/spline.cpp
  ${BBPK_DIR}/replay/synth.cpp ${BBPK_DIR}/replay/tb_303.cpp ${BBPK_DIR}/replay/wavpack.cpp)
set(BBPK_DEFS __STAND_ALONE__ __WINAMP__ __REPLAYER__ __MP3_CODEC__ __GSM_CODEC__ __AT3_CODEC__)
set_source_files_properties(${BBPK_DIR}/replay/wavpack.cpp PROPERTIES COMPILE_DEFINITIONS __GCC__)
if(SEL_AMIGA)
  # ProTrekkr's files are little-endian; its Swap_16/32/64 (replay/include/endianness.h) only swap with __BIG_ENDIAN__, which
  # m68k-amigaos-gcc does not predefine (it has __BYTE_ORDER__ only). Without it every multi-byte field loads byte-swapped (I427e1).
  list(APPEND BBPK_DEFS __BIG_ENDIAN__)
  add_library(bbmp_ptk_engine STATIC ${BBPK_ENGINE})
  target_include_directories(bbmp_ptk_engine PRIVATE "${BBPK_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/ptk_win" "${CMAKE_CURRENT_SOURCE_DIR}/ptk_zlib"
    "${PROJECT_SOURCE_DIR}/third_party/miniz")
  target_compile_options(bbmp_ptk_engine PRIVATE -w -fvisibility=hidden)
  target_compile_definitions(bbmp_ptk_engine PRIVATE ${BBPK_DEFS})
  set_target_properties(bbmp_ptk_engine PROPERTIES CXX_STANDARD 17)
  # I427e: an on-demand LoadSeg module (Players/sel_bbmp_ptk); the registry links only probe.c as the manifest.
  sel_add_plugin(bbmp_ptk 1 adapter.c shim.cpp probe.c AMIGA_MANIFEST probe.c
    THIRD_PARTY_INCLUDES "${BBPK_DIR};${CMAKE_CURRENT_SOURCE_DIR}/ptk_win;${CMAKE_CURRENT_SOURCE_DIR}/ptk_zlib;${PROJECT_SOURCE_DIR}/third_party/miniz")
  target_compile_options(sel_seg_bbmp_ptk PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
  target_compile_definitions(sel_seg_bbmp_ptk PRIVATE ${BBPK_DEFS})
  set_target_properties(sel_seg_bbmp_ptk PROPERTIES CXX_STANDARD 17)
  # The module links with the C driver: libstdc++ is named here. miniz comes from selecta_archive.
  target_link_libraries(sel_seg_bbmp_ptk PRIVATE bbmp_ptk_engine selecta_archive stdc++)
else()
  find_package(ZLIB REQUIRED)
  sel_add_plugin(bbmp_ptk 1 MODULE adapter.c shim.cpp probe.c ${BBPK_ENGINE}
    THIRD_PARTY_INCLUDES "${BBPK_DIR};${CMAKE_CURRENT_SOURCE_DIR}/ptk_win;${ZLIB_INCLUDE_DIRS}")
  target_compile_options(sel_module_bbmp_ptk PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
  target_compile_definitions(sel_module_bbmp_ptk PRIVATE ${BBPK_DEFS})
  target_link_libraries(sel_module_bbmp_ptk PRIVATE ZLIB::ZLIB ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
  set_target_properties(sel_module_bbmp_ptk PROPERTIES CXX_STANDARD 17)
endif()
