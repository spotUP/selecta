# The 68k `abi voices` output must equal the m68k golden (32-bit pointers).
# Inputs: -DVAMOS -DAMIGA_CLI -DGOLDEN
foreach(v VAMOS AMIGA_CLI GOLDEN)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_voices.cmake: -D${v} is required")
  endif()
endforeach()
execute_process(COMMAND "${VAMOS}" -C 68020 -- "${AMIGA_CLI}" abi voices
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "vamos abi voices: exit ${rc}\n${err}")
endif()
file(READ "${GOLDEN}" golden)
if(NOT out STREQUAL golden)
  message(FATAL_ERROR "abi voices differs from ${GOLDEN}\n${out}")
endif()
message(STATUS "[OK] sel_voices matches the m68k golden")
