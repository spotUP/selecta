# T14 driver: renders the test module with the host CLI and with the 68k CLI
# under vamos and requires byte-identical WAVs and identical counter lines
# (the mixer is integer-only, so both CPUs must agree bit for bit). When the
# golden hash file exists the SHA-256 must match it too.
# Inputs: -DVAMOS -DHOST_CLI -DAMIGA_CLI -DMOD -DOUT_DIR -DRESAMPLE -DROOT -DGOLDEN
# vamos maps long absolute host paths badly (they come back truncated), so
# every path handed to the 68k binary is relative to ROOT, the repository.
foreach(v VAMOS HOST_CLI AMIGA_CLI MOD OUT_DIR RESAMPLE ROOT)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_render.cmake: -D${v} is required")
  endif()
endforeach()
file(MAKE_DIRECTORY "${OUT_DIR}")
set(host_wav "${OUT_DIR}/host.${RESAMPLE}.wav")
set(amiga_wav "${OUT_DIR}/amiga.${RESAMPLE}.wav")
file(REMOVE "${host_wav}" "${amiga_wav}")

execute_process(
  COMMAND "${HOST_CLI}" render "${MOD}" --output "${host_wav}" --seconds 10 --rate 44100 --resample ${RESAMPLE}
  OUTPUT_VARIABLE host_out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "host render exited ${rc}\n${err}")
endif()

file(RELATIVE_PATH rel_mod "${ROOT}" "${MOD}")
file(RELATIVE_PATH rel_wav "${ROOT}" "${amiga_wav}")
execute_process(
  COMMAND "${VAMOS}" -C 68020 -m 8192 -- "${AMIGA_CLI}" render "${rel_mod}" --output "${rel_wav}"
          --seconds 10 --rate 44100 --resample ${RESAMPLE}
  WORKING_DIRECTORY "${ROOT}"
  OUTPUT_VARIABLE amiga_out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "vamos render exited ${rc}\n${err}")
endif()

string(REGEX MATCH "ticks=[^\n]*" host_line "${host_out}")
string(REGEX MATCH "ticks=[^\n]*" amiga_line "${amiga_out}")
if(NOT host_line MATCHES "^ticks=496 voice_ops=[1-9][0-9]* mixed_frames=441000$")
  message(FATAL_ERROR "host counters wrong: '${host_line}'")
endif()
if(NOT host_line STREQUAL amiga_line)
  message(FATAL_ERROR "counters differ\nhost:  ${host_line}\namiga: ${amiga_line}")
endif()

file(SIZE "${amiga_wav}" size)
if(NOT size EQUAL 1764044)
  message(FATAL_ERROR "68k WAV size ${size}, expected 1764044")
endif()
file(SHA256 "${host_wav}" host_hash)
file(SHA256 "${amiga_wav}" amiga_hash)
if(NOT host_hash STREQUAL amiga_hash)
  message(FATAL_ERROR "68k WAV differs from host WAV\nhost:  ${host_hash}\namiga: ${amiga_hash}")
endif()
if(GOLDEN AND EXISTS "${GOLDEN}")
  file(READ "${GOLDEN}" pinned)
  string(SUBSTRING "${pinned}" 0 64 pinned)
  if(NOT amiga_hash STREQUAL pinned)
    message(FATAL_ERROR "SHA-256 ${amiga_hash} differs from ${GOLDEN} (${pinned})")
  endif()
endif()
message(STATUS "[OK] 68k render == host render (${RESAMPLE}), sha256 ${amiga_hash}; ${amiga_line}")
