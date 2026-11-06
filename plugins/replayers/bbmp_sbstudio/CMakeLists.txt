# bbmp sbstudio replayer (I710v): SBStudio songs (.pac) through libpac 0.9.0 by Thomas Pfaff (ISC, third_party/libpac, the copy bbmp builds).
# adapter.c is written for Selecta; bbmp's GPL-3.0-or-later wrapper (csrc/synth/sbstudio_engine.c) is not used. libpac is plain C and ISC, so
# the module is core (no SELECTA_GPL_PLUGINS gate).
set(LIBPAC_DIR "${PROJECT_SOURCE_DIR}/third_party/libpac/src")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
set(LIBPAC_SRC ${LIBPAC_DIR}/effect.c ${LIBPAC_DIR}/error.c ${LIBPAC_DIR}/gus.c ${LIBPAC_DIR}/init.c ${LIBPAC_DIR}/load.c
  ${LIBPAC_DIR}/mix.c ${LIBPAC_DIR}/read.c ${LIBPAC_DIR}/seek.c)
sel_add_plugin(bbmp_sbstudio 1 MODULE adapter.c ${LIBPAC_SRC} THIRD_PARTY_INCLUDES "${LIBPAC_DIR}")
set_source_files_properties(${LIBPAC_SRC} PROPERTIES COMPILE_OPTIONS "-w")
