# I101 text UI panels (plan 2026-10-23-m1-3-viz-voices-playback.md, Phase 6).
# Drives the real shell with a key file that plays selecta-test-mk.mod and opens a panel,
# then compares the last dumped screen with the golden.
#   -DSELECTA=<exe> -DMOD=<selecta-test-mk.mod> -DKEYS=<keys> -DGOLDEN=<screen golden> -DWORK=<scratch dir>
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/root/songs")
file(COPY "${MOD}" DESTINATION "${WORK}/root/songs")
if(NOT EXISTS "${SELECTA}")
  message(FATAL_ERROR "selecta shell not built: ${SELECTA}")
endif()
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}/root" --keys "${KEYS}"
          --screen-dump "${WORK}/screen.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
file(READ "${WORK}/screen.txt" screen)
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "panel screen matches ${GOLDEN}")
