# I104d: note dots and phase view. Variables: SELECTA MOD KEYS GOLDEN WORK COUNTER (dot_rows|phase_points)
# Runs the real shell, plays selecta-test.mod (nuade, four voices), opens the panel with V and
# requires (1) the exit line `viz_marks dot_rows=N phase_points=M` to show COUNTER > 0, so the draw
# path ran, and (2) the last dumped screen to equal the golden.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/root/songs" "${WORK}/config")
file(COPY "${MOD}" DESTINATION "${WORK}/root/songs")
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}/root" --config-dir "${WORK}/config"
          --keys "${KEYS}" --screen-dump "${WORK}/screen.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "viz_marks dot_rows=([0-9]+) phase_points=([0-9]+)")
  message(FATAL_ERROR "no viz_marks line\nstdout:\n${out}")
endif()
set(dot_rows "${CMAKE_MATCH_1}")
set(phase_points "${CMAKE_MATCH_2}")
if(${COUNTER} LESS 1)
  message(FATAL_ERROR "${COUNTER} is 0: the draw path did not run (dot_rows=${dot_rows} phase_points=${phase_points})")
endif()
file(READ "${WORK}/screen.txt" screen)
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "${COUNTER}=${${COUNTER}}, screen matches ${GOLDEN}")
