# I195e: OPEN of a streamed URL answering 404 or 503 must report the HTTP failure (open_failed), not play the error
# body and not fail the probe as no_replayer. The fixture routes /stream_404 and /stream_503 serve an HTML error page
# with that status; before the fix the page entered the ring and the probe failed as no_replayer.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "fixture.test=http://127.0.0.1:${port}")
foreach(code 404 503)
  file(WRITE "${WORK}/i195e-${code}.sel" "OPEN http://fixture.test/stream_${code}\n@stream_live\n")
  execute_process(COMMAND "${CLI}" script "${WORK}/i195e-${code}.sel" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 20)
  set(all "${o}${e}")
  if(all MATCHES "no_replayer")
    message(FATAL_ERROR "I195e: ${code} reported no_replayer\n${all}")
  endif()
  if(NOT all MATCHES "ERROR open_failed")
    message(FATAL_ERROR "I195e: ${code} did not report open_failed\n${all}")
  endif()
  if(NOT all MATCHES "audio_bytes=0 audio_done=0 received=0")
    message(FATAL_ERROR "I195e: ${code} error body reached the ring (want received=0)\n${all}")
  endif()
  # the record sink must not tap the error page either
  execute_process(COMMAND "${CLI}" record "http://fixture.test/stream_${code}" "${WORK}/i195e-${code}.rec" --seconds 2
    OUTPUT_VARIABLE ro ERROR_VARIABLE re RESULT_VARIABLE rrc TIMEOUT 20)
  if(NOT ro MATCHES "recorded_bytes=0 record_taps=0")
    message(FATAL_ERROR "I195e: ${code} error body reached the record file\n${ro}${re}")
  endif()
endforeach()
message(STATUS "stream_status_ok")
