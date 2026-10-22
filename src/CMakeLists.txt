add_library(selecta_core STATIC
  core/version.c
  core/tag.c
  core/codec.c
  core/machine.c
  host/host.c
  host/mem.c
  host/log.c
  engine/spsc.c
  engine/engine.c
  engine/queue.c
  engine/voices.c
  engine/scheduler.c)
target_include_directories(selecta_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Fixed-point mixer and resamplers (no float, no allocation).
# The SINC table is generated at build time by a host tool (float and libm
# live only there); the Amiga cross build runs the host compiler for it.
if(SEL_AMIGA)
  find_program(SEL_HOST_CC NAMES cc clang gcc REQUIRED)
else()
  set(SEL_HOST_CC "${CMAKE_C_COMPILER}")
endif()
set(SEL_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
  OUTPUT "${SEL_GEN_DIR}/sinc_table.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SEL_GEN_DIR}"
  COMMAND "${SEL_HOST_CC}" -O1 -o "${SEL_GEN_DIR}/gen_sinc_table" "${PROJECT_SOURCE_DIR}/tools/gen_sinc_table.c" -lm
  COMMAND "${SEL_GEN_DIR}/gen_sinc_table" > "${SEL_GEN_DIR}/sinc_table.h"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/gen_sinc_table.c"
  VERBATIM)
add_library(selecta_mixer STATIC mixer/mixer.c mixer/resample.c "${SEL_GEN_DIR}/sinc_table.h")
target_include_directories(selecta_mixer PUBLIC ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/mixer)
target_include_directories(selecta_mixer PRIVATE "${SEL_GEN_DIR}")
target_link_libraries(selecta_core PUBLIC selecta_mixer)

# One clock source and one machine probe per platform. OS headers live only
# under src/core/<os>/ (RULES.md rule 3): C clock() plus sysctl (macOS) or
# /proc (Linux) on desktop, DateStamp and exec AttnFlags on Amiga.
target_include_directories(selecta_core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/core ${CMAKE_CURRENT_SOURCE_DIR}/host)
if(SEL_AMIGA)
  target_sources(selecta_core PRIVATE core/amiga/clock.c core/amiga/machine.c)
elseif(APPLE)
  target_sources(selecta_core PRIVATE host/clock.c core/macos/machine.c)
else()
  target_sources(selecta_core PRIVATE host/clock.c core/linux/machine.c)
endif()
