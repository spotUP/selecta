# bbmp smufi replayer (I710t): Picatune 1 songs (.smufi). The engine is bbmp's csrc/retro/smufi_engine.cpp (GPL-3.0-or-later), a clean-room
# synthesizer tracker replayer with its own resampler; no other library. GPL, so its own loadable module under SELECTA_GPL_PLUGINS; the MIT
# core never links it. Host only: C++17 engine with x87 long double and glibc maths, no 68k build (exemption in row I710t).
enable_language(CXX)
set(BBSMUFI_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_smufi")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_smufi 1 MODULE adapter.c shim.cpp "${BBSMUFI_DIR}/smufi_engine.cpp"
  THIRD_PARTY_INCLUDES "${BBSMUFI_DIR}")
set_source_files_properties("${BBSMUFI_DIR}/smufi_engine.cpp" shim.cpp PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_smufi PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_smufi PROPERTIES CXX_STANDARD 17)
