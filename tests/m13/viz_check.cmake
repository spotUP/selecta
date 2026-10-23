# T40: render the test module through selecta-cli with --viz-dump and compare the
# per-block taps (peaks, VU, bands, scope hashes, correlation) with viz.expected.
# Variables: CLI MOD OUT DUMP EXPECTED
get_filename_component(out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")
file(REMOVE "${OUT}" "${DUMP}")
execute_process(
  COMMAND "${CLI}" render "${MOD}" --output "${OUT}" --seconds 2 --viz-dump "${DUMP}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "viz_blocks=100\n")
  message(FATAL_ERROR "expected viz_blocks=100 (2000 ms / 20 ms), got:\n${out}")
endif()
if(NOT EXISTS "${DUMP}")
  message(FATAL_ERROR "no dump written to ${DUMP}")
endif()
file(READ "${DUMP}" got)
file(READ "${EXPECTED}" want)
if(NOT got STREQUAL want)
  message(FATAL_ERROR "viz dump differs from ${EXPECTED}; dump kept at ${DUMP}")
endif()
