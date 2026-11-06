# bbmp unitrk replayer (I710m): MikMod UNITRK modules (.uni) through libmikmod 3.3.14 (LGPL 2.1 or later, third_party/bbmp_unitrk), the engine
# bbmp uses for them. The module links it statically, so it is its own loadable module behind SELECTA_GPL_PLUGINS (same opt-in as the other
# LGPL engine, adplug); the MIT core never links it. Plain C99; Amiga status in row I710m.
set(BBUN_DIR "${PROJECT_SOURCE_DIR}/third_party/bbmp_unitrk")
set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
set(BBUN_SOURCES ${BBUN_DIR}/mmio/mmalloc.c ${BBUN_DIR}/mmio/mmerror.c ${BBUN_DIR}/mmio/mmio.c
  ${BBUN_DIR}/posix/strcasecmp.c ${BBUN_DIR}/loaders/load_uni.c ${BBUN_DIR}/drivers/drv_nos.c)
file(GLOB BBUN_PLAYER CONFIGURE_DEPENDS "${BBUN_DIR}/playercode/*.c")
sel_add_plugin(bbmp_unitrk 1 MODULE adapter.c ${BBUN_SOURCES} ${BBUN_PLAYER} THIRD_PARTY_INCLUDES "${BBUN_DIR}/include")
# Upstream code is not clean under the project's -Wall -Wextra -Werror -pedantic; third-party, so warnings are off.
set_source_files_properties(${BBUN_SOURCES} ${BBUN_PLAYER} PROPERTIES COMPILE_OPTIONS "-w;-std=gnu99"
  COMPILE_DEFINITIONS "HAVE_CONFIG_H;MIKMOD_BUILD")
