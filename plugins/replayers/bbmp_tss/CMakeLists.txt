# bbmp TSS replayer (I710af): T'SoundSystem songs (.tss MML text, .tsd compiled). The engine is bbmp's csrc/retro/tss_engine.cpp (BSD-3-Clause,
# a port of toyoshim's TSS), built unchanged with Selecta's glue (third_party/bbmp_tss/btss_glue.cpp). Permissive, so a core module (no GPL gate).
# Host only: C++17 engine (exemption in row I710af, follow-up I710afa).
enable_language(CXX)
set(BBTS_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_tss")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_tss 1 MODULE adapter.c "${BBTS_DIR}/btss_glue.cpp" THIRD_PARTY_INCLUDES "${BBTS_DIR}")
set_source_files_properties("${BBTS_DIR}/btss_glue.cpp" PROPERTIES COMPILE_OPTIONS "-w;-fvisibility=hidden")
target_link_libraries(sel_module_bbmp_tss PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_tss PROPERTIES CXX_STANDARD 17)
