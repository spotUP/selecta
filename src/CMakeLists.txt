add_library(selecta_core STATIC
  core/version.c
  core/tag.c
  core/codec.c
  core/machine.c
  core/config_dir.c
  protocol/parse.c
  host/host.c
  host/mem.c
  host/log.c
  engine/spsc.c
  engine/engine.c
  engine/queue.c
  engine/history.c
  engine/shuffle.c
  engine/view.c
  engine/prefs.c
  engine/options.c
  engine/format.c
  engine/voices.c
  engine/scheduler.c
  engine/viz.c
  engine/dsp_chain.c
  engine/timer.c)
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
add_custom_command(
  OUTPUT "${SEL_GEN_DIR}/viz_table.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SEL_GEN_DIR}"
  COMMAND "${SEL_HOST_CC}" -O1 -o "${SEL_GEN_DIR}/gen_viz_table" "${PROJECT_SOURCE_DIR}/tools/gen_viz_table.c" -lm
  COMMAND "${SEL_GEN_DIR}/gen_viz_table" > "${SEL_GEN_DIR}/viz_table.h"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/gen_viz_table.c"
  VERBATIM)
add_custom_command(
  OUTPUT "${SEL_GEN_DIR}/pitch_table.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SEL_GEN_DIR}"
  COMMAND "${SEL_HOST_CC}" -O1 -o "${SEL_GEN_DIR}/gen_pitch" "${PROJECT_SOURCE_DIR}/tools/gen_pitch.c" -lm
  COMMAND "${SEL_GEN_DIR}/gen_pitch" > "${SEL_GEN_DIR}/pitch_table.h"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/gen_pitch.c"
  VERBATIM)
add_custom_command(
  OUTPUT "${SEL_GEN_DIR}/dsp_coef.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SEL_GEN_DIR}"
  COMMAND "${SEL_HOST_CC}" -O1 -o "${SEL_GEN_DIR}/gen_dsp_coef" "${PROJECT_SOURCE_DIR}/tools/gen_dsp_coef.c" -lm
  COMMAND "${SEL_GEN_DIR}/gen_dsp_coef" > "${SEL_GEN_DIR}/dsp_coef.h"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/gen_dsp_coef.c"
  VERBATIM)
add_custom_command(
  OUTPUT "${SEL_GEN_DIR}/xfade_table.h"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SEL_GEN_DIR}"
  COMMAND "${SEL_HOST_CC}" -O1 -o "${SEL_GEN_DIR}/gen_xfade" "${PROJECT_SOURCE_DIR}/tools/gen_xfade.c" -lm
  COMMAND "${SEL_GEN_DIR}/gen_xfade" > "${SEL_GEN_DIR}/xfade_table.h"
  DEPENDS "${PROJECT_SOURCE_DIR}/tools/gen_xfade.c"
  VERBATIM)
target_sources(selecta_core PRIVATE "${SEL_GEN_DIR}/xfade_table.h" "${SEL_GEN_DIR}/viz_table.h" "${SEL_GEN_DIR}/pitch_table.h" "${SEL_GEN_DIR}/dsp_coef.h")
target_include_directories(selecta_core PRIVATE "${SEL_GEN_DIR}")
add_library(selecta_mixer STATIC mixer/mixer.c mixer/resample.c mixer/stage.c "${SEL_GEN_DIR}/sinc_table.h")
target_include_directories(selecta_mixer PUBLIC ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/mixer)
target_include_directories(selecta_mixer PRIVATE "${SEL_GEN_DIR}")
target_link_libraries(selecta_core PUBLIC selecta_mixer)
add_subdirectory(library)
target_link_libraries(selecta_core PUBLIC selecta_library)

# One clock source and one machine probe per platform. OS headers live only
# under src/core/<os>/ (RULES.md rule 3): C clock() plus sysctl (macOS) or
# /proc (Linux) on desktop, DateStamp and exec AttnFlags on Amiga.
target_include_directories(selecta_core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/core ${CMAKE_CURRENT_SOURCE_DIR}/host)
if(SEL_AMIGA)
  target_sources(selecta_core PRIVATE core/amiga/clock.c core/amiga/machine.c core/amiga/config_mkdir.c core/amiga/pal.c
    core/amiga/loop.c)
  # The interrupt and audio-task path: integer only (T54 no-float-interrupt). The object list is generated from
  # the target, so the test counts what the build produced. Phases 3 and 4 add paula, paula14 and ahi objects here.
  add_library(selecta_audio_irq OBJECT core/amiga/audio_int.c core/amiga/audio_task.c)
  target_include_directories(selecta_audio_irq PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_sources(selecta_core PRIVATE $<TARGET_OBJECTS:selecta_audio_irq>)
  file(GENERATE OUTPUT ${PROJECT_BINARY_DIR}/m2-interrupt-objects.txt
    CONTENT "$<JOIN:$<TARGET_OBJECTS:selecta_audio_irq>,\n>\n")
elseif(APPLE)
  target_sources(selecta_core PRIVATE host/clock.c core/macos/machine.c core/macos/config_mkdir.c core/macos/pal.c)
else()
  target_sources(selecta_core PRIVATE host/clock.c core/linux/machine.c core/macos/config_mkdir.c core/macos/pal.c)
endif()
