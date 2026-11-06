# bbmp skale replayer (I710ab): Skale Tracker songs (.skm). The engine is bbmp's csrc/synth/skale_engine.cpp + skale_fx.cpp (GPL-3.0-or-later)
# over the vendored libopenmpt (selecta_openmpt) with Luke Preiner's SKM loader (BSD 2-clause) patched in
# (third_party/patches/libopenmpt, applied by plugins/replayers/openmpt/CMakeLists.txt). GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine and libopenmpt have no 68k build (exemption in row I710ab,
# follow-up I710aba).
enable_language(CXX)
set(BBSKALE_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_skale")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_skale 1 MODULE adapter.c "${BBSKALE_DIR}/skale_engine.cpp" "${BBSKALE_DIR}/skale_fx.cpp"
  THIRD_PARTY_INCLUDES "${BBSKALE_DIR}" "${SELECTA_OPENMPT_DIR}")
set_source_files_properties("${BBSKALE_DIR}/skale_engine.cpp" "${BBSKALE_DIR}/skale_fx.cpp" PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_skale PRIVATE selecta_openmpt ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_skale PROPERTIES CXX_STANDARD 17)
