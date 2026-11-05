# bbmp fac replayer (I710ak): FAC SoundTracker (MSX, .mus). The engine is bbmp's csrc/fac/fac_cr.cpp (GPL-3.0-or-later,
# third_party/bbmp_fac) on the MIT emu8950 / emu2413 chip emulators already vendored for libkss. GPL, so its own loadable
# module under SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engine, no Amiga module path
# (exemption in ledger row I710ak, RULES 4a).
enable_language(CXX)
set(BBFAC_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_fac")
set(KSSMOD_DIR "${PROJECT_SOURCE_DIR}/third_party/libkss/modules")
add_library(bbmp_fac_chips STATIC EXCLUDE_FROM_ALL
  "${KSSMOD_DIR}/emu8950/emu8950.c" "${KSSMOD_DIR}/emu8950/emuadpcm.c" "${KSSMOD_DIR}/emu2413/emu2413.c")
target_include_directories(bbmp_fac_chips PUBLIC "${KSSMOD_DIR}/emu8950" "${KSSMOD_DIR}/emu2413")
target_compile_options(bbmp_fac_chips PRIVATE -w -fvisibility=hidden)
set_target_properties(bbmp_fac_chips PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_fac 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${BBFAC_DIR}")
target_compile_options(sel_module_bbmp_fac PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-w>)
target_link_libraries(sel_module_bbmp_fac PRIVATE bbmp_fac_chips ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_fac PROPERTIES CXX_STANDARD 17)
