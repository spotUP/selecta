# KPlayer v33 (third_party/kplayer, HippoPlayer-68k kpl14.s, MIT) for ProTracker M.K. modules on Amiga (I219).
# The replayer is 68000 assembly built the way HippoPlayer builds it (vasm -m68000 -no-opt -Fbin); the flat
# binary goes into a generated C array that the adapter jumps into. The output is byte-identical to HippoPlayer's
# `kpl`. asm-include/ supplies two NDK include names the bebbo NDK lacks.
find_program(SEL_VASM vasmm68k_mot HINTS /opt/homebrew/bin /usr/local/bin NO_CMAKE_FIND_ROOT_PATH)
if(NOT SEL_VASM)
  message(WARNING "vasmm68k_mot not found: the kplayer replayer (ProTracker .mod on Amiga) is not built")
  return()
endif()

set(KPL_DIR "${PROJECT_SOURCE_DIR}/third_party/kplayer")
set(KPL_BIN "${CMAKE_CURRENT_BINARY_DIR}/kpl.bin")
set(KPL_C "${CMAKE_CURRENT_BINARY_DIR}/kpl_blob.c")
add_custom_command(OUTPUT "${KPL_BIN}"
  COMMAND "${SEL_VASM}" -quiet -m68000 -no-opt -Fbin
    -I "${CMAKE_CURRENT_SOURCE_DIR}/asm-include" -I "${KPL_DIR}" -I "${AMIGA_GCC_ROOT}/m68k-amigaos/ndk-include"
    -o "${KPL_BIN}" "${KPL_DIR}/kpl14.s"
  DEPENDS "${KPL_DIR}/kpl14.s" "${KPL_DIR}/kpl_offsets.S" "${KPL_DIR}/mucro.i"
  COMMENT "Assembling KPlayer v33")
add_custom_command(OUTPUT "${KPL_C}"
  COMMAND "${CMAKE_COMMAND}" -DIN="${KPL_BIN}" -DOUT="${KPL_C}" -P "${CMAKE_CURRENT_SOURCE_DIR}/bin2c.cmake"
  DEPENDS "${KPL_BIN}" "${CMAKE_CURRENT_SOURCE_DIR}/bin2c.cmake"
  COMMENT "Embedding KPlayer v33")
sel_add_plugin(kplayer 1 adapter.c "${KPL_C}")
