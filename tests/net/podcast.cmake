# I725f: the podcast source, driven through selecta-cli against a local fixture.
#  1. sync of an RSS feed URL: 4 items, 3 with an enclosure = listing_rows=3, skipped_rows=1; entities and CDATA decoded;
#  2. search: with no Podcast Index key the search is off (message on stderr, no request); with a key and secret the
#     fixture sees X-Auth-Key, X-Auth-Date and a 40 hex Authorization (SHA-1 of key+secret+date, checked here by python);
#  3. every request carries the descriptive User-Agent (Selecta-Podcast/1 ...);
#  4. OPEN + PLAY streams the enclosure; SEEK sends a Range request with a non-zero start (HEAD first for the length);
#  5. nothing is left in the cache dir.
# -DCLI=<selecta-cli> -DWORK=<scratch dir> -DMP3=<tests/m3/stream/sine.mp3> -DICY=<icy_server.py>
set(PORT "${WORK}/pod-port")
set(DB "${WORK}/pod.db")
set(ICACHE "${WORK}/pod-cache")
file(REMOVE_RECURSE "${PORT}" "${PORT}.requests" "${DB}" "${WORK}/pod.raw" "${ICACHE}")
execute_process(COMMAND sh -c "python3 -I '${ICY}' '${PORT}' '${MP3}' 40 >/dev/null 2>&1 &")
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
if(sp STREQUAL "")
  message(FATAL_ERROR "I725f: fixture server did not start")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "icy.test=http://127.0.0.1:${sp},pod.test=http://127.0.0.1:${sp}")
set(ENV{SELECTA_CACHE_DIR} "${ICACHE}")
set(ENV{SELECTA_PODCAST_INDEX_API} "http://pod.test")

# 1. Sync a feed URL.
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 2000 --dump podcast "http://pod.test/pod/feed.xml"
  OUTPUT_VARIABLE so ERROR_VARIABLE se RESULT_VARIABLE src TIMEOUT 40)
if(NOT src EQUAL 0 OR NOT so MATCHES "listing_rows=3" OR NOT so MATCHES "skipped_rows=1")
  message(FATAL_ERROR "I725f: want 3 rows, 1 skipped (rc=${src})\n${so}${se}")
endif()
if(NOT so MATCHES "podcast_row=Fixture-Show/Episode-One/http://icy.test/pod/ep1.mp3\\|Podcast - Mon, 01 Jan 2024"
   OR NOT so MATCHES "Fixture-Show/Fish & Chips/http://icy.test/pod/ep2.mp3\\?x=1&y=2" OR NOT so MATCHES "Fixture-Show/Cdata Three/")
  message(FATAL_ERROR "I725f: rows, titles or enclosure URLs are wrong\n${so}")
endif()

# 2. Search: no key -> off; with key -> authenticated request and the found feed is read.
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 2000 podcast "search:fixture"
  OUTPUT_VARIABLE so2 ERROR_VARIABLE se2 RESULT_VARIABLE src2 TIMEOUT 40)
file(READ "${PORT}.requests" rq)
if(NOT se2 MATCHES "no Podcast Index key, search off" OR rq MATCHES "pod_search=")
  message(FATAL_ERROR "I725f: search without a key must be off and send nothing\n${se2}\n${rq}")
endif()
set(ENV{SELECTA_PODCAST_INDEX_KEY} "testkey")
set(ENV{SELECTA_PODCAST_INDEX_SECRET} "testsecret")
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 2000 --dump podcast "search:fixture"
  OUTPUT_VARIABLE so3 ERROR_VARIABLE se3 RESULT_VARIABLE src3 TIMEOUT 40)
if(NOT src3 EQUAL 0 OR NOT so3 MATCHES "listing_rows=3")
  message(FATAL_ERROR "I725f: search then feed read failed (rc=${src3})\n${so3}${se3}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "pod_search=fixture key=testkey date=[0-9]+ auth=40")
  message(FATAL_ERROR "I725f: search request lacks the key, date or 40 hex Authorization\n${rq}")
endif()
string(REGEX MATCH "pod_search=[^\n]* date=([0-9]+)" md "${rq}")
set(sdate "${CMAKE_MATCH_1}")
string(REGEX MATCH "pod_auth=([0-9a-f]+)" ma "${rq}")
set(sauth "${CMAKE_MATCH_1}")
execute_process(COMMAND python3 -I -c "import hashlib,sys; sys.exit(0 if hashlib.sha1(('testkeytestsecret'+sys.argv[1]).encode()).hexdigest()==sys.argv[2] else 1)" "${sdate}" "${sauth}"
  RESULT_VARIABLE hrc)
if(NOT hrc EQUAL 0)
  message(FATAL_ERROR "I725f: Authorization is not SHA-1(key+secret+date): ${sauth} date ${sdate}")
endif()

# 3. Descriptive User-Agent on every request.
string(REGEX MATCHALL "pod_ua=" ua_all "${rq}")
string(REGEX MATCHALL "pod_ua=selecta-podcast/1 " ua_ok "${rq}")
list(LENGTH ua_all n_all)
list(LENGTH ua_ok n_ok)
if(n_all EQUAL 0 OR NOT n_all EQUAL n_ok)
  message(FATAL_ERROR "I725f: ${n_all} requests, only ${n_ok} with the descriptive User-Agent")
endif()

# 4. Play, seek.
file(WRITE "${WORK}/pod.sel"
  "@dump ${WORK}/pod.raw\nOPEN podcast:Fixture-Show/Episode-One/http://icy.test/pod/ep1.mp3\nPLAY\n@render 22050\n@sleep 300\nSEEK 400\n@render 22050\n@sleep 300\n@render 22050\n@stream_source\n@stream_live\nSTOP\n")
execute_process(COMMAND "${CLI}" script "${WORK}/pod.sel" --db "${DB}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 40)
if(NOT rc EQUAL 0 OR o MATCHES "ERROR" OR e MATCHES "ERROR")
  message(FATAL_ERROR "I725f: script failed rc=${rc}\n${o}${e}")
endif()
if(NOT o MATCHES "STREAMSOURCE resolves=[1-9]")
  message(FATAL_ERROR "I725f: the episode was not resolved through the source\n${o}")
endif()
if(NOT o MATCHES "STREAMLIVE opens=[1-9]")
  message(FATAL_ERROR "I725f: the enclosure did not open through sel_live\n${o}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "pod_full=head" OR NOT rq MATCHES "pod_range=[1-9][0-9]*")
  message(FATAL_ERROR "I725f: want a HEAD for the length and a Range request with a non-zero start after SEEK\n${rq}")
endif()
file(READ "${WORK}/pod.raw" hex HEX)
if(NOT hex MATCHES "[1-9a-f]")
  message(FATAL_ERROR "I725f: the render is silent")
endif()

# 5. No file left in the cache dir.
file(GLOB_RECURSE left "${ICACHE}/*")
if(left)
  message(FATAL_ERROR "I725f: files left in the cache dir: ${left}")
endif()
message(STATUS "podcast_ok")
