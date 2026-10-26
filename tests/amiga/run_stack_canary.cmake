# Driver for amiga-stack-canary (row I376): the 68k bottom-of-stack canary under vamos.
# Case "ok": normal run on a 4 KiB swapped stack, exit 0, no canary error line.
# Case "overflow": the body overruns the stack, the exit check prints the canary error line and exits non-zero.
# Inputs: -DVAMOS -DCANARY_TEST; WORKING_DIRECTORY must be the repo root.
foreach(v VAMOS CANARY_TEST)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_stack_canary.cmake: -D${v} is required")
  endif()
endforeach()

function(run_case mode out_var rc_var)
  execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -- "${CANARY_TEST}" ${mode}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
  set(${out_var} "${out}${err}" PARENT_SCOPE)
  set(${rc_var} "${rc}" PARENT_SCOPE)
endfunction()

run_case(ok all rc)
if(NOT rc EQUAL 0 OR NOT all MATCHES "canary_ok=1 swaps=1\n" OR all MATCHES "stack overflow")
  message(FATAL_ERROR "normal run: exit '${rc}', expected exit 0, canary_ok=1 and no canary error\n${all}")
endif()

run_case(overflow all rc)
if(rc EQUAL 0 OR NOT all MATCHES "amiga_start: stack overflow, canary at the stack bottom overwritten")
  message(FATAL_ERROR "overflow run: exit '${rc}', expected non-zero and the canary error line\n${all}")
endif()
message(STATUS "[OK] stack canary: normal run clean, overflow reported")
