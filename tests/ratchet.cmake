# T8: the number of verbs the engine answers with not_implemented may
# never rise. The allowed count lives in tests/ratchet/unimplemented.txt;
# a plan that implements verbs lowers it in the same commit.
execute_process(COMMAND "${CLI}" commands --unimplemented
  OUTPUT_VARIABLE out RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli commands --unimplemented exited ${rc}")
endif()
string(REGEX REPLACE "\n$" "" out "${out}")
string(REGEX MATCHALL "\n" nl "${out}")
list(LENGTH nl live)
if(NOT out STREQUAL "")
  math(EXPR live "${live} + 1")
endif()
if(NOT EXISTS "${RATCHET}")
  message(FATAL_ERROR "ratchet file missing: ${RATCHET} (live count ${live})")
endif()
file(READ "${RATCHET}" allowed)
string(STRIP "${allowed}" allowed)
if(live GREATER allowed)
  message(FATAL_ERROR "unimplemented verbs rose: ${live} live, ${allowed} allowed:\n${out}")
endif()
message(STATUS "unimplemented verbs: ${live} live, ${allowed} allowed")
