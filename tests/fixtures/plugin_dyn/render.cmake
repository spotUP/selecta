# I427a reachability test: the fixture module is found through $SELECTA_PLUGIN_PATH and selecta-cli renders a
# *.seldyn song with it. Proves WIRED (the plugins listing names the module path, dyn_loaded=1, the render reports
# replayer=dyntone) and the output (SHA-256 of the WAV, computed by an independent python script from the ramp
# formula: left (3 n mod 2000) - 1000, right the negative, 22050 frames at 44100 Hz, then silence padded to the 2 s of --seconds; verified byte for byte in python).
# Variables: CLI GOOD SONG OUT
set(ENV{SELECTA_PLUGIN_PATH} "${GOOD}")
execute_process(COMMAND "${CLI}" plugins --stats OUTPUT_VARIABLE list ERROR_VARIABLE lerr RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "plugins exited ${rc}\n${list}\n${lerr}")
endif()
if(NOT list MATCHES "dyntone kind=1 version=1 origin=${GOOD}/sel_dyn_tone\\.(dylib|so)\n")
  message(FATAL_ERROR "plugins listing lacks the module with its path:\n${list}")
endif()
if(NOT list MATCHES "dyn_loaded=1\ndyn_rejected=0\n")
  message(FATAL_ERROR "stats lack dyn_loaded=1 dyn_rejected=0:\n${list}")
endif()
file(REMOVE "${OUT}")
execute_process(COMMAND "${CLI}" render "${SONG}" --output "${OUT}" --seconds 2 --rate 44100
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
string(REGEX REPLACE "\n$" "" out "${out}")
if(NOT out MATCHES "^replayer=dyntone render_calls=[1-9][0-9]* rendered_frames=22050$")
  message(FATAL_ERROR "render counters wrong:\n${out}")
endif()
file(SHA256 "${OUT}" hash)
if(NOT hash STREQUAL "a317a07a234758f2f034b1b7ff39055f0be7640f0fad6b49af8858a2a726be47")
  message(FATAL_ERROR "WAV SHA-256 ${hash} differs from the pinned ramp")
endif()
