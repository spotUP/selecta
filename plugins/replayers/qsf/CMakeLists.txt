# QSF replayer (I704, bbmp backend Highly Quixotic): Capcom QSound songs (.qsf, .miniqsf with .qsflib) through the Highly
# Quixotic core (third_party/highly_quixotic, GPL-3.0, plain C) and psflib (third_party/psflib, MIT) behind a RENDER
# adapter. GPL, so it only ever builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON; the core binaries never
# link it. Host only: the Amiga build has no GPL module path (exemption in ledger row I704, RULES 4a).
set(HQ_DIR "${PROJECT_SOURCE_DIR}/third_party/highly_quixotic")
set(PSFLIB_DIR "${PROJECT_SOURCE_DIR}/third_party/psflib")
find_package(ZLIB REQUIRED)

add_library(selecta_qsf_core STATIC
  "${HQ_DIR}/kabuki.c" "${HQ_DIR}/qmix.c" "${HQ_DIR}/qsound.c" "${HQ_DIR}/qsound_ctr.c" "${HQ_DIR}/z80.c"
  "${PSFLIB_DIR}/psflib.c")
target_include_directories(selecta_qsf_core SYSTEM PUBLIC "${HQ_DIR}" "${PSFLIB_DIR}")
target_compile_definitions(selecta_qsf_core PRIVATE EMU_COMPILE EMU_LITTLE_ENDIAN HAVE_STDINT_H)
target_compile_options(selecta_qsf_core PRIVATE -w -fvisibility=hidden -fgnu89-inline)
target_link_libraries(selecta_qsf_core PUBLIC ZLIB::ZLIB)
set_target_properties(selecta_qsf_core PROPERTIES C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(qsf 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${HQ_DIR}" "${PSFLIB_DIR}")
target_link_libraries(sel_module_qsf PRIVATE selecta_qsf_core)
