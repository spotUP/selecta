# bbmp ntgs replayer (I710p): Noise Tracker GS 1 modules of the Apple IIGS. The engine is bbmp's csrc/iigs/ntgs_cr.cpp
# (GPL-3.0-or-later, third_party/bbmp_iigs) on the Ensoniq 5503 emulation doc5503.h. GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine, no Amiga module path (exemption in row I710p).
enable_language(CXX)
set(BBIIGS_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_iigs")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_ntgs 1 MODULE adapter.c shim.cpp "${BBIIGS_DIR}/ntgs_cr.cpp" THIRD_PARTY_INCLUDES "${BBIIGS_DIR}")
target_compile_options(sel_module_bbmp_ntgs PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_ntgs PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_ntgs PROPERTIES CXX_STANDARD 17)
