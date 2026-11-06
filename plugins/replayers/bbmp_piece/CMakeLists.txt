# bbmp piece replayer (I710s): P/ECE music driver songs (Aquaplus, .pmd) on bbmp's clean-room engine csrc/piece/piece.cpp
# (GPL-3.0-or-later, third_party/bbmp_piece). GPL, so its own loadable module under SELECTA_GPL_PLUGINS; the MIT core never links it.
# Host only: C++17 engine, no Amiga module path (exemption in row I710s).
enable_language(CXX)
set(BBPIECE_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_piece")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_piece 1 MODULE adapter.c shim.cpp "${BBPIECE_DIR}/piece.cpp" THIRD_PARTY_INCLUDES "${BBPIECE_DIR}")
target_compile_options(sel_module_bbmp_piece PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_piece PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_piece PROPERTIES CXX_STANDARD 17)
