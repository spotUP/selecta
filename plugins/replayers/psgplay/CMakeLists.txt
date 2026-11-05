# PSG play replayer (I707): Atari ST SNDH through Fredrik Noring's psgplay (third_party/psgplay, GPL-2.0), a second SNDH
# backend beside sc68 (plugin id sndh). Loadable module under SELECTA_GPL_PLUGINS only; host only (Amiga exemption in row I707).
# The TOS image and Musashi opcode tables are vendored pre-generated (third_party/psgplay/generated), so no m68k-elf-gcc.
set(PSG_DIR "${PROJECT_SOURCE_DIR}/third_party/psgplay")
set(SEL_PSG_SOURCES
  lib/ice/ice.c lib/psgplay/psgplay.c lib/psgplay/sndh.c
  lib/internal/bit.c lib/internal/fifo.c lib/internal/string.c
  lib/toslibc/lib/unicode-atari.c lib/toslibc/lib/unicode-utf8.c
  lib/atari/bus.c lib/atari/cpu.c lib/atari/dac.c lib/atari/device.c lib/atari/exception-vector.c lib/atari/fdc.c
  lib/atari/glue.c lib/atari/irq-trace.c lib/atari/machine.c lib/atari/mfp.c lib/atari/mixer.c lib/atari/mmu.c
  lib/atari/mmu-trace.c lib/atari/psg.c lib/atari/ram.c lib/atari/rom.c lib/atari/shifter.c lib/atari/sound.c
  lib/atari/system-variable.c
  lib/cf2149/module/cf2149.c lib/cf68901/module/cf68901.c lib/cf300588/module/cf300588-sound.c
  lib/m68k/m68kcpu.c generated/m68k/m68kops.c)
list(TRANSFORM SEL_PSG_SOURCES PREPEND "${PSG_DIR}/")
add_library(selecta_psgplay STATIC ${SEL_PSG_SOURCES})
target_include_directories(selecta_psgplay SYSTEM PUBLIC "${PSG_DIR}/generated" "${PSG_DIR}/include"
  "${PSG_DIR}/lib/cf2149/include" "${PSG_DIR}/lib/cf68901/include" "${PSG_DIR}/lib/cf300588/include"
  "${PSG_DIR}/lib/toslibc/include")
target_compile_definitions(selecta_psgplay PRIVATE _GNU_SOURCE)
target_compile_options(selecta_psgplay PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_psgplay PROPERTIES C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(psgplay 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${PSG_DIR}/include" "${PSG_DIR}/lib/toslibc/include")
target_link_libraries(sel_module_psgplay PRIVATE selecta_psgplay)
