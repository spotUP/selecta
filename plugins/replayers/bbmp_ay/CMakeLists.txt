# bbmp ay replayer (I709a): ZX Spectrum Flash Tracker (.fls), Fuxoft AY Language (.fxm, .amad) and Sound Tracker
# compiled by Raxoft (AY file "ZXAYSTRC", .strc). The engines are bbmp's csrc/ay (GPL-3.0-or-later, third_party/bbmp_ay,
# Ay_Emul credit in COPYING) on the Ayumi chip emulator (MIT, third_party/ayumi). GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS; the MIT core never links it. Host only: C++17 engines and no Amiga module path (exemption in
# ledger row I709a, RULES 4a).
enable_language(CXX)
set(BBAY_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_ay")
set(AYUMI_DIR "${PROJECT_SOURCE_DIR}/third_party/ayumi")
add_library(bbmp_ay_engine STATIC EXCLUDE_FROM_ALL
  "${BBAY_DIR}/aychip.cpp" "${BBAY_DIR}/fls_engine.cpp" "${BBAY_DIR}/fxm_engine.cpp" "${BBAY_DIR}/stc_cr.cpp" "${AYUMI_DIR}/ayumi.c")
target_include_directories(bbmp_ay_engine PUBLIC "${BBAY_DIR}" "${AYUMI_DIR}")
target_compile_options(bbmp_ay_engine PRIVATE -w -fvisibility=hidden)
set_target_properties(bbmp_ay_engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(bbmp_ay 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${BBAY_DIR}")
target_link_libraries(sel_module_bbmp_ay PRIVATE bbmp_ay_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_ay PROPERTIES CXX_STANDARD 17)
