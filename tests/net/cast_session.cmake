# I892e1: CASTV2 session against the TLS fixture receiver (tests/net/cast_server.py, self-signed certificate).
#  -DTEST=<cast_session_test> -DWORK=<scratch dir> -DSRV=<cast_server.py>
# Asserts: the self-signed TLS handshake and CONNECT; 3 heartbeat rounds (3 PONGs back, so the sender PINGs and reads
# the PONG); the receiver's own PING answered with a PONG; RECEIVER_STATUS parsed (requestId, first application,
# transportId, volume); and that the fixture logged CONNECT, 3+1 PINGs/PONGs and one GET_STATUS (the code ran).
set(PORT "${WORK}/cast-port")
file(REMOVE "${PORT}" "${PORT}.requests")
execute_process(COMMAND sh -c "python3 -I '${SRV}' '${PORT}' 30 '${WORK}/cast-cert' >/dev/null 2>&1 &")
foreach(i RANGE 100)
  if(EXISTS "${PORT}")
    file(READ "${PORT}" sp)
    if(NOT sp STREQUAL "")
      break()
    endif()
  endif()
  execute_process(COMMAND sleep 0.1)
endforeach()
string(STRIP "${sp}" sp)
if(NOT sp MATCHES "^[0-9]+$")
  message(FATAL_ERROR "I892e1: fixture did not start (${sp})")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
execute_process(COMMAND "${TEST}" 127.0.0.1 ${sp} OUTPUT_VARIABLE so ERROR_VARIABLE se RESULT_VARIABLE rc TIMEOUT 30)
if(NOT rc EQUAL 0)
  file(READ "${PORT}.requests" rq)
  message(FATAL_ERROR "I892e1: cast session failed rc=${rc}\n${so}${se}\nfixture saw:\n${rq}")
endif()
foreach(want
    "cast_open=0\n" "cast_heartbeats=3\n" "cast_pongs=3\n" "cast_pings_answered=1\n" "cast_status=0\n"
    "cast_type=RECEIVER_STATUS\n" "cast_request=1\n" "cast_apps=1\n" "cast_app=CC1AD845\n"
    "cast_name=Default Media Receiver\n" "cast_transport=web-5\n" "cast_session=sess-1\n" "cast_volume=0.50\n"
    "cast_muted=0\n")
  string(FIND "${so}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "I892e1: missing '${want}'\n${so}")
  endif()
endforeach()
# The fixture flushes each line as it reads it; the sender closed the link, so the log is complete.
execute_process(COMMAND sleep 0.5)
file(READ "${PORT}.requests" rq)
string(REGEX MATCHALL "tp.heartbeat PING\n" pings "${rq}")
string(REGEX MATCHALL "tp.heartbeat PONG\n" pongs "${rq}")
list(LENGTH pings np)
list(LENGTH pongs nq)
if(NOT np EQUAL 3 OR NOT nq EQUAL 1)
  message(FATAL_ERROR "I892e1: fixture saw ${np} PING / ${nq} PONG, want 3 / 1\n${rq}")
endif()
foreach(want "tp.connection CONNECT\n" "cast.receiver GET_STATUS\n")
  string(FIND "${rq}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "I892e1: fixture log lacks '${want}'\n${rq}")
  endif()
endforeach()
message(STATUS "cast_session_ok")
