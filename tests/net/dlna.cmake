# I894a3: the dlna source, driven through selecta-cli against the UPnP fixture (tests/net/upnp_server.py).
#  1. sync of the device description URL: the tree is walked recursively through paged Browse: container "1" has
#     150 items over 2 pages plus a sub-container with 3 items, so listing_rows=153 and db_songs=153 (container "2"
#     is empty);
#  2. sync "discover" (unicast SSDP to the fixture through SELECTA_DLNA_SSDP): the same 153 rows, one M-SEARCH;
#  3. OPEN + PLAY one song from the sub-container streams its res URL: the fixture saw the media GET, the stream went
#     through the source and sel_live, and the render holds non-zero samples (the played-samples sentinel).
# -DCLI=<selecta-cli> -DWORK=<scratch dir> -DMP3=<tests/m3/stream/sine.mp3> -DSRV=<upnp_server.py>
set(PORT "${WORK}/dlna-port")
set(DB "${WORK}/dlna.db")
set(DB2 "${WORK}/dlna2.db")
file(REMOVE_RECURSE "${PORT}" "${PORT}.requests" "${DB}" "${DB2}" "${WORK}/dlna.raw")
execute_process(COMMAND sh -c "python3 -I '${SRV}' '${PORT}' 60 '${MP3}' >/dev/null 2>&1 &")
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
  message(FATAL_ERROR "I894a3: fixture did not start (${sp})")
endif()
set(UDP "${CMAKE_MATCH_1}")
set(HTTP "${CMAKE_MATCH_2}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")

# 1. Scan by description URL.
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 2000 dlna "http://127.0.0.1:${HTTP}/desc.xml"
  OUTPUT_VARIABLE so ERROR_VARIABLE se RESULT_VARIABLE src TIMEOUT 60)
if(NOT src EQUAL 0 OR NOT so MATCHES "listing_rows=153 " OR NOT so MATCHES "skipped_rows=0 " OR NOT so MATCHES "db_songs=153")
  message(FATAL_ERROR "I894a3: want 153 rows (150 + 3 in the sub-container) rc=${src}\n${so}${se}")
endif()
file(READ "${PORT}.requests" rq)
foreach(b "0:0:100" "1:0:100" "1:100:100" "1$sub:0:100")
  string(FIND "${rq}" "browse=${b}\n" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "I894a3: fixture never saw browse=${b}\n${rq}")
  endif()
endforeach()

# 2. Discover.
set(ENV{SELECTA_DLNA_SSDP} "127.0.0.1:${UDP}")
execute_process(COMMAND "${CLI}" library sync --db "${DB2}" --now 2000 dlna discover
  OUTPUT_VARIABLE so2 ERROR_VARIABLE se2 RESULT_VARIABLE src2 TIMEOUT 60)
if(NOT src2 EQUAL 0 OR NOT so2 MATCHES "listing_rows=153 " OR NOT so2 MATCHES "db_songs=153"
   OR NOT se2 MATCHES "dlna: server Fixture Media & Music control http://127.0.0.1:${HTTP}/upnp/cd/control")
  message(FATAL_ERROR "I894a3: discover scan failed rc=${src2}\n${so2}${se2}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "ssdp_search=urn:schemas-upnp-org:device:MediaServer:1")
  message(FATAL_ERROR "I894a3: no M-SEARCH reached the fixture\n${rq}")
endif()

# 2b. I894a4: a Synology-like tree lists the same 15 files under Artist, Album, Genre and Folder views. With a folder
# container the scan walks only root > Music > Folder (5 Browse requests: 0, m, f, f$d1, f$d2); without one it walks
# everything and the repeated res URLs are dropped. Both store each file once; the folder walk asks for fewer pages.
function(syn_sync tree outvar)
  set(sport "${WORK}/dlna-${tree}-port")
  set(sdb "${WORK}/dlna-${tree}.db")
  file(REMOVE_RECURSE "${sport}" "${sport}.requests" "${sdb}")
  execute_process(COMMAND sh -c "python3 -I '${SRV}' '${sport}' 30 '${MP3}' '${tree}' >/dev/null 2>&1 &")
  foreach(i RANGE 50)
    if(EXISTS "${sport}")
      file(READ "${sport}" sp2)
      if(NOT sp2 STREQUAL "")
        break()
      endif()
    endif()
    execute_process(COMMAND sleep 0.1)
  endforeach()
  string(STRIP "${sp2}" sp2)
  if(NOT sp2 MATCHES "^([0-9]+) ([0-9]+)$")
    message(FATAL_ERROR "I894a4: ${tree} fixture did not start (${sp2})")
  endif()
  execute_process(COMMAND "${CLI}" library sync --db "${sdb}" --now 2000 dlna "http://127.0.0.1:${CMAKE_MATCH_2}/desc.xml"
    OUTPUT_VARIABLE so3 ERROR_VARIABLE se3 RESULT_VARIABLE src3 TIMEOUT 60)
  if(NOT src3 EQUAL 0 OR NOT so3 MATCHES "listing_rows=15 " OR NOT so3 MATCHES "db_songs=15")
    message(FATAL_ERROR "I894a4: ${tree}: want each of the 15 files stored once rc=${src3}\n${so3}${se3}")
  endif()
  file(READ "${sport}.requests" rq3)
  string(REGEX MATCHALL "browse=[^\n]*" bl "${rq3}")
  list(LENGTH bl cnt)
  set(${outvar} ${cnt} PARENT_SCOPE)
  set(${outvar}_log "${se3}" PARENT_SCOPE)
endfunction()
syn_sync("synology" folder_requests)
syn_sync("synology-plain" full_requests)
if(NOT folder_requests EQUAL 5 OR NOT folder_requests_log MATCHES "walking the folder view")
  message(FATAL_ERROR "I894a4: the folder walk wanted 5 Browse requests, saw ${folder_requests}\n${folder_requests_log}")
endif()
if(NOT full_requests_log MATCHES "walking the whole tree" OR NOT full_requests_log MATCHES "45 repeated res URLs"
   OR NOT full_requests GREATER folder_requests)
  message(FATAL_ERROR "I894a4: the fallback must walk everything (${full_requests} requests) and drop repeats\n${full_requests_log}")
endif()

# 3. Play one song of the sub-container.
set(KEY "dlna:Artist-201/Track-201/http://127.0.0.1:${HTTP}/media/201.mp3?a=1&b=2")
file(WRITE "${WORK}/dlna.sel"
  "@dump ${WORK}/dlna.raw\nOPEN ${KEY}\nPLAY\n@render 22050\n@sleep 300\n@render 22050\n@stream_source\n@stream_live\nSTOP\n")
execute_process(COMMAND "${CLI}" script "${WORK}/dlna.sel" --db "${DB}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 40)
if(NOT rc EQUAL 0 OR o MATCHES "ERROR" OR e MATCHES "ERROR")
  message(FATAL_ERROR "I894a3: script failed rc=${rc}\n${o}${e}")
endif()
if(NOT o MATCHES "STREAMSOURCE resolves=[1-9]")
  message(FATAL_ERROR "I894a3: the song was not resolved through the source\n${o}")
endif()
if(NOT o MATCHES "STREAMLIVE opens=[1-9]")
  message(FATAL_ERROR "I894a3: the res URL did not open through sel_live\n${o}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "media_get=/media/201.mp3")
  message(FATAL_ERROR "I894a3: the fixture never served the media file\n${rq}")
endif()
file(READ "${WORK}/dlna.raw" hex HEX)
if(NOT hex MATCHES "[1-9a-f]")
  message(FATAL_ERROR "I894a3: the render is silent (played samples = 0)")
endif()
message(STATUS "dlna_ok")
