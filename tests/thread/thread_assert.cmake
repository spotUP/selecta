# Runs thread_assert_test in MODE. EXPECT_ABORT: the child must die from the
# assertion (signal, no exit code 0/1/2) and stderr must name it. Otherwise
# the child must exit 0.
execute_process(COMMAND "${TEST}" "${MODE}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(EXPECT_ABORT)
  if(rc STREQUAL "0" OR rc STREQUAL "1" OR rc STREQUAL "2")
    message(FATAL_ERROR "${MODE}: expected an assertion abort, got rc=${rc}\n${err}")
  endif()
  if(NOT err MATCHES "audio_marked")
    message(FATAL_ERROR "${MODE}: aborted (rc=${rc}) without the audio-thread assertion text\n${err}")
  endif()
else()
  if(NOT rc STREQUAL "0")
    message(FATAL_ERROR "${MODE}: expected a clean exit, rc=${rc}\n${err}")
  endif()
endif()
