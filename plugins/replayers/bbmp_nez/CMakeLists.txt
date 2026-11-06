# bbmp nez replayer (I710w): SGC and Game Boy GBR songs through libnezplug (third_party/libnezplug, Unlicense / MIT-0, plain C) plus our own adapter.
# Core module, no SELECTA_GPL_PLUGINS gate.
set(NEZ_DIR "${PROJECT_SOURCE_DIR}/third_party/libnezplug/src")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
file(GLOB NEZ_SRC ${NEZ_DIR}/cpu/kmz80/kmdmg.c ${NEZ_DIR}/cpu/kmz80/kmevent.c ${NEZ_DIR}/cpu/kmz80/kmr800.c
  ${NEZ_DIR}/cpu/kmz80/kmz80.c ${NEZ_DIR}/cpu/kmz80/kmz80c.c ${NEZ_DIR}/cpu/kmz80/kmz80t.c ${NEZ_DIR}/cpu/kmz80/kmz80u.c
  ${NEZ_DIR}/device/*.c ${NEZ_DIR}/device/nes/*.c ${NEZ_DIR}/device/opl/*.c
  ${NEZ_DIR}/format/*.c ${NEZ_DIR}/logtable/*.c ${NEZ_DIR}/opltable/*.c)
sel_add_plugin(bbmp_nez 1 MODULE adapter.c ${NEZ_SRC} THIRD_PARTY_INCLUDES "${NEZ_DIR}/include")
set_source_files_properties(${NEZ_SRC} PROPERTIES COMPILE_OPTIONS "-w")
