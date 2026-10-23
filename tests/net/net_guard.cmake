# T67 net-guard: run selecta-cli fetch under SELECTA_NET_ALLOW / SELECTA_NET_MAP
# (contract in fixture_server.py). Needs -DCLI -DWORK; WORK/port from the fixture.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
execute_process(COMMAND "${CLI}" fetch http://203.0.113.1/x
  OUTPUT_VARIABLE blocked ERROR_VARIABLE err1 RESULT_VARIABLE rc1)
if(NOT blocked MATCHES "net_blocked=1")
  message(FATAL_ERROR "T67: non-loopback fetch not blocked (rc=${rc1})\nstdout: ${blocked}\nstderr: ${err1}")
endif()
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=http://127.0.0.1:${port}")
execute_process(COMMAND "${CLI}" fetch http://devilbox.uprough.net/api/health
  OUTPUT_VARIABLE mapped ERROR_VARIABLE err2 RESULT_VARIABLE rc2)
if(NOT mapped MATCHES "net_mapped=1")
  message(FATAL_ERROR "T67: mapped host not reached (rc=${rc2})\nstdout: ${mapped}\nstderr: ${err2}")
endif()
file(STRINGS "${WORK}/requests.jsonl" lines)
list(LENGTH lines n)
if(NOT n EQUAL 1)
  message(FATAL_ERROR "T67: fixture request log has ${n} lines, want 1")
endif()
