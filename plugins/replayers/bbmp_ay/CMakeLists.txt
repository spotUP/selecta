# bbmp ay replayer (I709a): ZX Spectrum Flash Tracker (.fls), Fuxoft AY Language (.fxm, .amad) and Sound Tracker
# compiled by Raxoft (AY file "ZXAYSTRC", .strc) and Beepola (.bbsong, I1223b). The engines are bbmp's csrc/ay (GPL-3.0-or-later, third_party/bbmp_ay,
# Ay_Emul credit in COPYING) on the Ayumi chip emulator (MIT, third_party/ayumi). GPL, so its own loadable module under
# SELECTA_GPL_PLUGINS; the MIT core never links it. A loadable module on the host; on the Amiga (I1223c) an on-demand LoadSeg
# module Players/sel_bbmp_ay (as bbmp_ptk), Beepola's second pass renders synchronously (BBAY_NO_THREADS, no std::thread).
enable_language(CXX)
set(BBAY_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_ay")
set(AYUMI_DIR "${PROJECT_SOURCE_DIR}/third_party/ayumi")
add_library(bbmp_ay_engine STATIC EXCLUDE_FROM_ALL
  "${BBAY_DIR}/aychip.cpp" "${BBAY_DIR}/fls_engine.cpp" "${BBAY_DIR}/fxm_engine.cpp" "${BBAY_DIR}/stc_cr.cpp" "${BBAY_DIR}/beepola_engine.cpp" "${AYUMI_DIR}/ayumi.c")
target_include_directories(bbmp_ay_engine PUBLIC "${BBAY_DIR}" "${AYUMI_DIR}")
target_compile_options(bbmp_ay_engine PRIVATE -w -fvisibility=hidden)
set_target_properties(bbmp_ay_engine PROPERTIES CXX_STANDARD 17)
if(NOT SEL_AMIGA) # -fPIC makes m68k gcc 6.5 hit an internal compiler error (print_operand_address2), as in pxtone
  set_target_properties(bbmp_ay_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
if(SEL_AMIGA)
  # The engines use C++ exceptions as control flow (song end, refusal); the Amiga toolchain defaults to -fno-exceptions.
  # BBAY_INT_SOUND (I1223c): Beepola's sound path in integers; the float one costs hours per song on an FPU-less 68020.
  target_compile_definitions(bbmp_ay_engine PRIVATE BBAY_NO_THREADS BBAY_INT_SOUND)
  target_compile_options(bbmp_ay_engine PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
endif()

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
if(SEL_AMIGA)
  sel_add_plugin(bbmp_ay 1 adapter.c shim.cpp probe.c AMIGA_MANIFEST probe.c THIRD_PARTY_INCLUDES "${BBAY_DIR}")
  target_compile_definitions(sel_seg_bbmp_ay PRIVATE BBAY_NO_THREADS)
  target_compile_options(sel_seg_bbmp_ay PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions> $<$<COMPILE_LANGUAGE:CXX>:-w>)
  set_target_properties(sel_seg_bbmp_ay PROPERTIES CXX_STANDARD 17)
  target_link_libraries(sel_seg_bbmp_ay PRIVATE bbmp_ay_engine stdc++)
else()
sel_add_plugin(bbmp_ay 1 MODULE adapter.c shim.cpp probe.c THIRD_PARTY_INCLUDES "${BBAY_DIR}")
find_package(Threads REQUIRED) # beepola_engine.cpp runs a second pass on std::thread
target_link_libraries(sel_module_bbmp_ay PRIVATE bbmp_ay_engine Threads::Threads ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_bbmp_ay PROPERTIES CXX_STANDARD 17)
endif()
