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
