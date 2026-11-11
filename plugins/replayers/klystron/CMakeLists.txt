# Klystrack replayer (I708a): klystrack chiptune songs (.kt) through the klystron sound engine (kometbomb, MIT,
# third_party/klystron) with a small SDL stand-in (third_party/klystron/sdl_shim). MIT-compatible: stays in a
# SELECTA_RELEASE_MIT_ONLY bundle. The engine uses float in a few places, so SEL_RP_NEEDS_FLOAT hides it on an Amiga without an FPU.
# Amiga: an on-demand LoadSeg module (I1305m, the bbmp_ptk pattern as hively I427e4): Players/sel_klystron holds the adapter and
# the engine (with its sqrtf, which pulled mathieeesingtrans.library into the exe start-up); the registry links only probe.c as
# the manifest (the "cyd!song" signature probe, no replayer code in any Amiga executable).
set(KLYS_DIR "${PROJECT_SOURCE_DIR}/third_party/klystron")
file(GLOB KLYS_SRC "${KLYS_DIR}/*.c")
add_library(klystron_engine STATIC EXCLUDE_FROM_ALL ${KLYS_SRC})
target_include_directories(klystron_engine PUBLIC "${KLYS_DIR}" "${KLYS_DIR}/sdl_shim")
target_compile_definitions(klystron_engine PUBLIC NOSDL_MIXER STEREOOUTPUT)
target_compile_options(klystron_engine PRIVATE -w)

if(SEL_AMIGA)
  # No -fPIC on 68k; soft-float objects (see organya).
  set_target_properties(klystron_engine PROPERTIES EXCLUDE_FROM_ALL OFF)
  sel_add_plugin(klystron 1 adapter.c probe.c AMIGA_MANIFEST probe.c THIRD_PARTY_INCLUDES "${KLYS_DIR}" "${KLYS_DIR}/sdl_shim")
  target_link_libraries(sel_seg_klystron PRIVATE klystron_engine m)
  # music.h (upstream) uses unnamed unions, which -pedantic -Werror rejects in the adapter's translation unit.
  target_compile_options(sel_seg_klystron PRIVATE -Wno-pedantic)
else()
  set_target_properties(klystron_engine PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
  set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
  sel_add_plugin(klystron 1 MODULE adapter.c probe.c THIRD_PARTY_INCLUDES "${KLYS_DIR}" "${KLYS_DIR}/sdl_shim")
  target_link_libraries(sel_module_klystron PRIVATE klystron_engine m)
  # music.h (upstream) uses anonymous unions: scope the C11-extension and pedantic relaxations to this module only.
  target_compile_options(sel_module_klystron PRIVATE -Wno-pedantic -Wno-c11-extensions)
  set_property(TARGET sel_module_klystron PROPERTY SEL_MODULE_MIT ON)
endif()
