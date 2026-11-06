# bbmp iigs replayer (I710ad): SoundSmith songs of the Apple IIGS. The engine is bbmp's csrc/iigs (GPL-3.0-or-later,
# third_party/bbmp_iigs: ss_cr.cpp on the Ensoniq 5503 emulation doc5503.h). GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine, no Amiga module path (exemption in row I710ad).
enable_language(CXX)
set(BBIIGS_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_iigs")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_iigs 1 MODULE adapter.c shim.cpp "${BBIIGS_DIR}/ss_cr.cpp" THIRD_PARTY_INCLUDES "${BBIIGS_DIR}")
target_compile_options(sel_module_bbmp_iigs PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_iigs PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_iigs PROPERTIES CXX_STANDARD 17)
