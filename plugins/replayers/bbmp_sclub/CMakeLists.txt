# bbmp sclub replayer (I710ac): Sound Club and Sound Club 2 (.sn, .sn2). The engine is bbmp's csrc/misc/sclub_cr.cpp
# (GPL-3.0-or-later, third_party/bbmp_sclub). GPL, so its own loadable module under SELECTA_GPL_PLUGINS; the MIT core never
# links it. Host only: C++17 engine, no Amiga module path (exemption in ledger row I710ac, RULES 4a).
enable_language(CXX)
set(BBSCLUB_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_sclub")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_sclub 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${BBSCLUB_DIR}")
target_compile_options(sel_module_bbmp_sclub PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_sclub PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_sclub PROPERTIES CXX_STANDARD 17)
