# I684c: the real shell must block each turn in exactly one place. Runs `selecta --output <id> <song>` with a key file
# and reads the shell's `shell_wait_ms_min/max` line (the UI wait it passed on every turn).
#   -DSELECTA=<exe> -DMOD=<song> -DKEYS=<keys> -DOUTPUT=<id> -DWANT_MIN=<n> -DWANT_MAX=<n>
# paula-shadow (pump never blocks): every wait is SHELL_TURN_MS (20). ahi-shadow (pump paces): every wait is 0.
execute_process(COMMAND "${SELECTA}" --output "${OUTPUT}" --keys "${KEYS}" "${MOD}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "shell_wait_ms_min=([0-9]+) shell_wait_ms_max=([0-9]+)")
  message(FATAL_ERROR "no shell_wait_ms line\n${out}")
endif()
set(lo ${CMAKE_MATCH_1})
set(hi ${CMAKE_MATCH_2})
if(NOT out MATCHES "ui_waits=([0-9]+)" OR CMAKE_MATCH_1 LESS 10)
  message(FATAL_ERROR "the shell barely looped\n${out}")
endif()
if(NOT out MATCHES "songs_opened=1 ")
  message(FATAL_ERROR "the song did not open (the new code never ran with a song)\n${out}")
endif()
if(NOT lo EQUAL WANT_MIN OR NOT hi EQUAL WANT_MAX)
  message(FATAL_ERROR "output ${OUTPUT}: UI wait ${lo}..${hi} ms, want ${WANT_MIN}..${WANT_MAX}")
endif()
message(STATUS "output ${OUTPUT}: UI wait ${lo}..${hi} ms")
