# T47, render half: `render <song> --dsp crossfeed` must run the plugin
# (counter line `dsp=crossfeed blocks=N`, N > 0), change the audio against a
# render without it, and match the golden SHA-256 when one is committed.
# Variables: CLI MOD OUT PLAIN GOLDEN
get_filename_component(out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")
file(REMOVE "${OUT}" "${PLAIN}")
execute_process(
  COMMAND "${CLI}" render "${MOD}" --output "${OUT}" --seconds 2 --dsp crossfeed
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render --dsp crossfeed exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "dsp=crossfeed blocks=[1-9][0-9]*\n")
  message(FATAL_ERROR "expected 'dsp=crossfeed blocks=N' with N > 0, got:\n${out}")
endif()
execute_process(
  COMMAND "${CLI}" render "${MOD}" --output "${PLAIN}" --seconds 2
  OUTPUT_VARIABLE out2 ERROR_VARIABLE err2 RESULT_VARIABLE rc2)
if(NOT rc2 EQUAL 0)
  message(FATAL_ERROR "plain render exited ${rc2}\n${err2}")
endif()
if(out2 MATCHES "dsp=")
  message(FATAL_ERROR "plain render must not print a dsp line:\n${out2}")
endif()
file(SHA256 "${OUT}" hash)
file(SHA256 "${PLAIN}" hash_plain)
if(hash STREQUAL hash_plain)
  message(FATAL_ERROR "crossfeed render is identical to the plain render (${hash})")
endif()
if(GOLDEN AND EXISTS "${GOLDEN}")
  file(READ "${GOLDEN}" pinned)
  string(SUBSTRING "${pinned}" 0 64 pinned)
  if(NOT hash STREQUAL pinned)
    message(FATAL_ERROR "SHA-256 ${hash} differs from ${GOLDEN} (${pinned})")
  endif()
else()
  message(STATUS "golden hash absent: ${GOLDEN} (computed ${hash})")
endif()
