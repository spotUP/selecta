# T15: render with selecta-cli (linear), then compare against stock nuade.
# Variables: CLI AB MOD OUT
get_filename_component(out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")
file(REMOVE "${OUT}")
execute_process(
  COMMAND "${CLI}" render "${MOD}" --output "${OUT}" --seconds 10 --rate 44100 --resample linear
  RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render exited ${rc}\n${err}")
endif()
execute_process(COMMAND "${AB}" "${MOD}" "${OUT}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
message(STATUS "${out}")
if(NOT rc EQUAL 0 OR NOT out MATCHES "^windows=500 correlation=")
  message(FATAL_ERROR "nuade-ab failed (rc=${rc}): ${out}${err}")
endif()
