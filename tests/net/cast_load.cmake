# I892e2: LAUNCH + LOAD against the TLS fixture receiver. -DTEST=<cast_load_test> -DWORK=<scratch dir>
# -DSRV=<cast_server.py> -DFILE=<song>
# Asserts the cast-load row: the fixture received LOAD with the URL Selecta serves, fetched it, and the md5 of the
# fetched bytes equals the file's; cast_loads is 1; the Range, HEAD, wrong-token and path-escape requests behave;
# PAUSE / SEEK / PLAY / SET_VOLUME reach the receiver and their MEDIA_STATUS / RECEIVER_STATUS come back.
set(PORT "${WORK}/cast-load-port")
file(REMOVE "${PORT}" "${PORT}.requests")
execute_process(COMMAND sh -c "python3 -I '${SRV}' '${PORT}' 40 '${WORK}/cast-load-cert' >/dev/null 2>&1 &")
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
  message(FATAL_ERROR "I892e2: fixture did not start (${sp})")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
execute_process(COMMAND "${TEST}" 127.0.0.1 ${sp} "${FILE}" OUTPUT_VARIABLE so ERROR_VARIABLE se RESULT_VARIABLE rc TIMEOUT 40)
file(READ "${PORT}.requests" rq)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I892e2: cast load failed rc=${rc}\n${so}${se}\nfixture saw:\n${rq}")
endif()
file(SIZE "${FILE}" fsize)
file(MD5 "${FILE}" fmd5)
execute_process(COMMAND python3 -I -c "import hashlib,sys;print(hashlib.md5(open(sys.argv[1],'rb').read()[10:20]).hexdigest())"
                "${FILE}" OUTPUT_VARIABLE rmd5 OUTPUT_STRIP_TRAILING_WHITESPACE)
math(EXPR bsent "${fsize} + 10") # full body plus the 10-byte Range request
foreach(want
    "cast_open=0\n" "cast_launch=0\n" "cast_transport=web-5\n" "cast_local_ip=127.0.0.1\n" "cast_url_prefix=http://\n"
    "cast_load=0\n" "cast_loads=1\n" "cast_http_requests=5\n" "cast_session_id=1\n" "cast_state=PLAYING\n"
    "cast_time=0.0\n" "cast_duration=100.0\n" "cast_http_bytes=${bsent}\n"
    "cast_pause=0\n" "cast_state_paused=PAUSED\n" "cast_seek=0\n" "cast_seek_time=42.5\n" "cast_play=0\n"
    "cast_state_playing=PLAYING\n" "cast_volume=0\n" "cast_volume_level=0.25\n")
  string(FIND "${so}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "I892e2: missing '${want}'\n${so}\nfixture saw:\n${rq}")
  endif()
endforeach()
# What the receiver itself saw: the LOAD, the fetched bytes' md5, and the other requests.
# The Range request only reaches the file's bytes 10..19, so its fetched length is not logged; its md5 and Content-Range are.
foreach(want
    "urn:x-cast:com.google.cast.receiver LAUNCH\n" "urn:x-cast:com.google.cast.media LOAD\n"
    "http full 200 ${fmd5} ${fsize}\n" "http head 200 ${fsize}\n"
    "http range 206 ${rmd5} bytes 10-19/${fsize}\n" "http wrong 404\n" "http escape 404\n"
    "media LOAD id=- state=PLAYING cur=0.0\n" "media PAUSE id=1 state=PAUSED" "media SEEK id=1 state=PAUSED cur=42.5\n"
    "media PLAY id=1 state=PLAYING cur=42.5\n" "urn:x-cast:com.google.cast.receiver SET_VOLUME\n")
  string(FIND "${rq}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "I892e2: fixture log lacks '${want}'\n${rq}")
  endif()
endforeach()
message(STATUS "cast_load_ok")
