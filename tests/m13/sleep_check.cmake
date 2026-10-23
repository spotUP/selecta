# T45: sleep timer and alarm through `selecta-cli script --clock fake=07:29:59`.
# Variables: CLI SCRIPTS SONGS
execute_process(COMMAND "${CLI}" script "${SCRIPTS}/sleep.sel" --clock fake=07:29:59
  WORKING_DIRECTORY "${SONGS}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "sleep: script exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
foreach(want "STATE paused" "ERROR range ALARM" "OPTIONS_CHANGED alarm 07:30")
  if(NOT out MATCHES "${want}")
    message(FATAL_ERROR "sleep: expected /${want}/ in:\n${out}\nstderr:\n${err}")
  endif()
endforeach()
# After the alarm second the transport plays again.
string(REGEX MATCH "STATE paused.*STATE playing" resumed "${out}")
if(resumed STREQUAL "")
  message(FATAL_ERROR "sleep: no STATE playing after STATE paused:\n${out}")
endif()

# A cancelled timer and a cancelled alarm: the transport keeps playing and no alarm event fires.
execute_process(COMMAND "${CLI}" script "${SCRIPTS}/sleep_cancel.sel" --clock fake=07:29:59
  WORKING_DIRECTORY "${SONGS}" OUTPUT_VARIABLE out2 ERROR_VARIABLE err2 RESULT_VARIABLE rc2)
if(NOT rc2 EQUAL 0)
  message(FATAL_ERROR "sleep cancel: script exited ${rc2}\nstdout:\n${out2}\nstderr:\n${err2}")
endif()
if(out2 MATCHES "STATE paused")
  message(FATAL_ERROR "sleep cancel: SLEEP_TIMER 0 did not cancel:\n${out2}")
endif()
if(NOT out2 MATCHES "OPTIONS_CHANGED alarm off")
  message(FATAL_ERROR "sleep cancel: no alarm off event:\n${out2}")
endif()
string(REGEX MATCHALL "STATE playing" plays "${out2}")
list(LENGTH plays nplays)
if(NOT nplays EQUAL 1)
  message(FATAL_ERROR "sleep cancel: ALARM off still fired (${nplays} STATE playing):\n${out2}")
endif()
