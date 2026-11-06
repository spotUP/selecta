# bbmp musicline replayer (I710n): Musicline Editor (.ml). bbmp's csrc/musicline: replay routine LGPL-2.1-or-later (port of
# mlineplayer102.asm), Paula mixer and front end GPL-3.0-or-later, tables MIT (NostalgicPlayer), all in third_party/bbmp_musicline.
# The mixer is GPL, so the module sits behind SELECTA_GPL_PLUGINS like bbmp_fac; the MIT core never links it. Host only: C++17
# engine, no Amiga module path (exemption in ledger row I710n, RULES 4a).
enable_language(CXX)
set(BBML_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_musicline")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_musicline 1 MODULE adapter.c shim.cpp "${BBML_DIR}/mline_load.cpp" "${BBML_DIR}/mline_play.cpp"
  THIRD_PARTY_INCLUDES "${BBML_DIR}")
target_compile_options(sel_module_bbmp_musicline PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_musicline PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_musicline PROPERTIES CXX_STANDARD 17)
