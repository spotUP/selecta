# T16: `selecta-cli play <song> --output coreaudio --seconds 2` plays through
# the default output device and prints `callbacks=K frames=F` from the
# coreaudio plugin's render-callback counters. Asserts K >= 1 and
# F >= 79380 (2 s x 44100 x 0.9). The CLI exits 77 when the machine has no
# default output device; the test then prints the skip marker that
# SKIP_REGULAR_EXPRESSION matches. Inputs: -DCLI, -DMOD.
execute_process(COMMAND "${CLI}" play "${MOD}" --output coreaudio --seconds 2
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(rc EQUAL 77)
  message(STATUS "coreaudio-play: no output device available")
  return()
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli play exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "callbacks=([0-9]+) frames=([0-9]+)")
  message(FATAL_ERROR "no 'callbacks=K frames=F' line\nstdout:\n${out}")
endif()
set(k ${CMAKE_MATCH_1})
set(f ${CMAKE_MATCH_2})
if(k LESS 1)
  message(FATAL_ERROR "render callback never ran: callbacks=${k}")
endif()
if(f LESS 79380)
  message(FATAL_ERROR "too few frames: frames=${f} < 79380")
endif()
message(STATUS "callbacks=${k} frames=${f}")
