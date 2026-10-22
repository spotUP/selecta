# T9 driver: runs the 68k selecta-cli under vamos and checks that it agrees
# with the host build. Inputs: -DVAMOS -DHOST_CLI -DAMIGA_CLI -DGOLDEN.
# roundtrip output must equal the host output byte for byte; abi output must
# equal the m68k golden (32-bit pointers, big-endian layout).
foreach(v VAMOS HOST_CLI AMIGA_CLI GOLDEN)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_vamos.cmake: -D${v} is required")
  endif()
endforeach()

function(run_amiga out_var)
  execute_process(COMMAND "${VAMOS}" -C 68020 -- "${AMIGA_CLI}" ${ARGN}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "vamos ${ARGN}: exit ${rc}\n${err}")
  endif()
  set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

execute_process(COMMAND "${HOST_CLI}" roundtrip OUTPUT_VARIABLE host_rt RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "host roundtrip: exit ${rc}")
endif()
run_amiga(amiga_rt roundtrip)
if(NOT host_rt STREQUAL amiga_rt)
  message(FATAL_ERROR "roundtrip differs\nhost:  ${host_rt}amiga: ${amiga_rt}")
endif()

run_amiga(amiga_abi abi)
file(READ "${GOLDEN}" golden)
if(NOT amiga_abi STREQUAL golden)
  message(FATAL_ERROR "abi differs from ${GOLDEN}\n${amiga_abi}")
endif()
message(STATUS "[OK] amiga roundtrip matches host, abi matches m68k golden")
