# T94 record part (I196, F36): `selecta-cli record <url> <file>` against slow_server.py (advertises 40 copies of
# sine.mp3, sends 60% and holds the connection like a radio stream). The recorded file must equal the bytes the
# server sent (sha256, via cmake), and the record sentinel (record_taps) must show the sink ran. Three endings:
# a byte limit, the time limit, and SIGTERM (STOP); each leaves a file that is exactly the served prefix.
# -DCLI=<selecta-cli> -DWORK=<scratch dir> -DMP3=<tests/m3/stream/sine.mp3> -DSLOW=<slow_server.py>
set(REPEAT 40)
set(PERCENT 60)
file(SIZE "${MP3}" one)
math(EXPR total "${one} * ${REPEAT}")
math(EXPR part "${total} * ${PERCENT} / 100")
# Expected: the first `part` bytes of sine.mp3 repeated.
file(REMOVE "${WORK}/rec-expected.bin" "${WORK}/rec-all.bin")
execute_process(COMMAND sh -c "i=0; while [ $i -lt ${REPEAT} ]; do cat '${MP3}'; i=$((i+1)); done | head -c ${part} > '${WORK}/rec-expected.bin'")
file(SHA256 "${WORK}/rec-expected.bin" want)
file(REMOVE "${WORK}/slow-port")
execute_process(COMMAND sh -c "python3 -I '${SLOW}' '${WORK}/slow-port' '${MP3}' ${REPEAT} ${PERCENT} 40 >/dev/null 2>&1 &")
foreach(i RANGE 50)
  if(EXISTS "${WORK}/slow-port")
    file(READ "${WORK}/slow-port" sp)
    if(NOT sp STREQUAL "")
      break()
    endif()
  endif()
  execute_process(COMMAND sleep 0.1)
endforeach()
string(STRIP "${sp}" sp)
if(sp STREQUAL "")
  message(FATAL_ERROR "I196: slow server did not start")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "slow.test=http://127.0.0.1:${sp}")
set(URL "http://slow.test/radio.mp3")

# 1. byte limit: exactly 50000 bytes, equal to the served prefix.
file(REMOVE "${WORK}/rec1.mp3")
execute_process(COMMAND "${CLI}" record "${URL}" "${WORK}/rec1.mp3" --bytes 50000 OUTPUT_VARIABLE o1 ERROR_VARIABLE e1
  RESULT_VARIABLE rc1 TIMEOUT 20)
if(NOT rc1 EQUAL 0 OR NOT o1 MATCHES "RECORD stream_bytes=([0-9]+) recorded_bytes=50000 record_taps=([1-9][0-9]*) end=byte_limit")
  message(FATAL_ERROR "I196: byte-limit record failed rc=${rc1}\n${o1}${e1}")
endif()
file(SIZE "${WORK}/rec1.mp3" sz1)
execute_process(COMMAND head -c 50000 "${WORK}/rec-expected.bin" OUTPUT_FILE "${WORK}/rec1-expected.bin")
execute_process(COMMAND cmp "${WORK}/rec1.mp3" "${WORK}/rec1-expected.bin" RESULT_VARIABLE cmp1)
if(NOT sz1 EQUAL 50000 OR NOT cmp1 EQUAL 0)
  message(FATAL_ERROR "I196: byte-limit file is ${sz1} bytes or differs from the served prefix")
endif()

# 2. time limit on the stalled stream: everything the server sent (60%), nothing more.
file(REMOVE "${WORK}/rec2.mp3")
execute_process(COMMAND "${CLI}" record "${URL}" "${WORK}/rec2.mp3" --seconds 3 OUTPUT_VARIABLE o2 ERROR_VARIABLE e2
  RESULT_VARIABLE rc2 TIMEOUT 20)
file(SHA256 "${WORK}/rec2.mp3" got2)
if(NOT rc2 EQUAL 0 OR NOT o2 MATCHES "recorded_bytes=${part} record_taps=([1-9][0-9]*) end=time_limit" OR NOT got2 STREQUAL want)
  message(FATAL_ERROR "I196: time-limit record failed rc=${rc2} want ${part} bytes sha ${want}, got ${got2}\n${o2}${e2}")
endif()

# 3. STOP: SIGTERM mid-stream ends the recording cleanly and promptly.
file(REMOVE "${WORK}/rec3.mp3" "${WORK}/rec3.out")
string(TIMESTAMP t0 "%s")
execute_process(COMMAND sh -c "'${CLI}' record '${URL}' '${WORK}/rec3.mp3' > '${WORK}/rec3.out' 2>&1 & pid=$!; sleep 3; kill -TERM $pid; wait $pid"
  RESULT_VARIABLE rc3 TIMEOUT 20)
string(TIMESTAMP t1 "%s")
math(EXPR secs "${t1} - ${t0}")
file(READ "${WORK}/rec3.out" o3)
file(SHA256 "${WORK}/rec3.mp3" got3)
if(NOT rc3 EQUAL 0 OR NOT o3 MATCHES "recorded_bytes=${part} record_taps=([1-9][0-9]*) end=stop" OR NOT got3 STREQUAL want OR secs GREATER 8)
  message(FATAL_ERROR "I196: STOP record failed rc=${rc3} secs=${secs}\n${o3}")
endif()
message(STATUS "I196 recorded ${part} bytes equal to the served prefix (sha256 ${want})")
message(STATUS "stream_record_ok")
