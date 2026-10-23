# I104c: key-level engine state. Variables: SELECTA MOD KEYS WANT WORK
# Runs the real selecta shell with a keys file and compares the engine_state line it prints on exit
#   engine_state pitch_cents=P tempo_percent=T mute_mask=M mutes=N
# with WANT. Keys: } { move PITCH by 100 cents, > < move TEMPO by 5 percent, S solos the cursor
# voice, 0 clears the solo. Every number is the engine's own state after the keys went through
# the key bindings, the text UI and the protocol.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/root/songs" "${WORK}/config")
file(COPY "${MOD}" DESTINATION "${WORK}/root/songs")
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}/root" --config-dir "${WORK}/config"
          --keys "${KEYS}" --screen-dump "${WORK}/screen.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "engine_state ([^\n]*)\n")
  message(FATAL_ERROR "no engine_state line\nstdout:\n${out}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL "${WANT}")
  message(FATAL_ERROR "engine state\n  got:  ${CMAKE_MATCH_1}\n  want: ${WANT}")
endif()
message(STATUS "${CMAKE_MATCH_1}")
