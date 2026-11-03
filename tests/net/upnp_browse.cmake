# I894a2: ContentDirectory Browse against the python fixture (tests/net/upnp_server.py).
#  -DTEST=<upnp_browse_test> -DWORK=<scratch dir> -DSRV=<upnp_server.py>
# Asserts: root has 2 containers (title entity decoded, childCount); container "1" has 151 children (1 container + 150
# items) delivered over exactly 2 pages (the server caps a page at 100); one item's fields exact (double-escaped title,
# album, res URL, protocolInfo, duration 0:03:25.500 = 205500 ms); and the fixture saw exactly the 3 Browse calls
# 0:0, 1:0, 1:100 (the code ran, one SOAP request per page).
set(PORT "${WORK}/upnp-browse-port")
file(REMOVE "${PORT}" "${PORT}.requests")
execute_process(COMMAND sh -c "python3 -I '${SRV}' '${PORT}' 30 >/dev/null 2>&1 &")
foreach(i RANGE 50)
  if(EXISTS "${PORT}")
    file(READ "${PORT}" sp)
    if(NOT sp STREQUAL "")
      break()
    endif()
  endif()
  execute_process(COMMAND sleep 0.1)
endforeach()
string(STRIP "${sp}" sp)
if(NOT sp MATCHES "^([0-9]+) ([0-9]+)$")
  message(FATAL_ERROR "I894a2: fixture did not start (${sp})")
endif()
set(HTTP "${CMAKE_MATCH_2}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
execute_process(COMMAND "${TEST}" "http://127.0.0.1:${HTTP}/upnp/cd/control" OUTPUT_VARIABLE so ERROR_VARIABLE se
                RESULT_VARIABLE rc TIMEOUT 30)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I894a2: browse failed rc=${rc}\n${so}${se}")
endif()
foreach(want
    "upnp_root n=2 total=2\n"
    "upnp_root_container id=1 title=Rock & Roll children=151\n"
    "upnp_root_container id=2 title=Jazz children=0\n"
    "upnp_item id=1\\$042\n"
    "upnp_title=Song 042 & More\n"
    "upnp_artist=Artist 042\n"
    "upnp_album=Album <A>\n"
    "upnp_ms=205500\n"
    "upnp_res=http://127.0.0.1:${HTTP}/media/042.mp3\\?a=1&b=2\n"
    "upnp_proto=http-get:\\*:audio/mpeg:DLNA.ORG_PN=MP3\n"
    "upnp_pages=2 containers=1 items=150 total=151\n")
  if(NOT so MATCHES "${want}")
    message(FATAL_ERROR "I894a2: missing '${want}' in\n${so}")
  endif()
endforeach()
file(READ "${PORT}.requests" rq)
string(REGEX MATCHALL "browse=[^\n]*" b_all "${rq}")
list(LENGTH b_all nb)
if(NOT nb EQUAL 3 OR NOT rq MATCHES "browse=0:0:100\nbrowse=1:0:100\nbrowse=1:100:100\n")
  message(FATAL_ERROR "I894a2: want Browse calls 0:0, 1:0, 1:100, got ${nb}\n${rq}")
endif()
message(STATUS "upnp_browse_ok")
