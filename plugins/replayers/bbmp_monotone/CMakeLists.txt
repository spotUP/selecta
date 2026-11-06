# bbmp Monotone replayer (I710ao): MONOTONE songs (.mon) through bbmp's csrc/pcmusic/monotone.cpp (GPL-3.0-or-later port of the public-domain tracker player).
# The vendored file carries a GPL header, so its own loadable module under SELECTA_GPL_PLUGINS. Host only: C++17 engine (exemption in row I710ao).
enable_language(CXX)
set(BBMON_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_monotone")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
# -ffp-contract=off: bbmp checks the speaker output tick for tick against the original program; no fused multiply-add.
sel_add_plugin(bbmp_monotone 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${BBMON_DIR}")
set_source_files_properties(shim.cpp PROPERTIES COMPILE_OPTIONS "-w;-ffp-contract=off")
target_link_libraries(sel_module_bbmp_monotone PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_monotone PROPERTIES CXX_STANDARD 17)
