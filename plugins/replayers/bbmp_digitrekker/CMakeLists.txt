# bbmp Digitrekker replayer (I710ap): DigiTrekker songs (.dtm) through bbmp's csrc/pcmusic/digitrekker.cpp (GPL-3.0-or-later, written by bbmp from the format document).
# The vendored file carries a GPL header, so its own loadable module under SELECTA_GPL_PLUGINS. Host only: C++17 engine (exemption in row I710ap).
enable_language(CXX)
set(BBDTM_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_digitrekker")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
# -ffp-contract=off: bbmp checks the output against DigiTrekker 3.10; no fused multiply-add.
sel_add_plugin(bbmp_digitrekker 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${BBDTM_DIR}")
set_source_files_properties(shim.cpp PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_digitrekker PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_digitrekker PROPERTIES CXX_STANDARD 17)
