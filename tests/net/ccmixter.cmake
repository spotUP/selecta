# I725g: the ccMixter source, driven through selecta-cli against a local fixture (recorded-style query API answer).
#  1. sync reads the fixture's one query answer (4 uploads): an upload under a non-CC licence and one with no MP3 file are
#     skipped. Rows: 101 (CC BY-NC 4.0) and 102 (CC NC-SAMPLING+ 1.0) = listing_rows=2, skipped_rows=2, net_requests=1
#     (the sentinel that the plugin ran: one query per sync, the server is not burdened); each row shows its own licence in
#     the format column and has the upload-page backlink;
#  2. every request carries the descriptive User-Agent (Selecta-CcMixter/1 ...);
#  3. OPEN ccmixter:<id>/.. + PLAY asks api/query?ids=<id>, streams the download URL (rewritten to the base) via sel_live;
#  4. nothing is left in the cache dir (audio and JSON are never kept).
# -DCLI=<selecta-cli> -DWORK=<scratch dir> -DMP3=<tests/m3/stream/sine.mp3> -DICY=<icy_server.py>
set(PORT "${WORK}/cc-port")
set(DB "${WORK}/cc.db")
set(CCACHE "${WORK}/cc-cache")
file(REMOVE_RECURSE "${PORT}" "${PORT}.requests" "${DB}" "${WORK}/cc.raw" "${CCACHE}")
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
  message(FATAL_ERROR "I725g: fixture server did not start")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "icy.test=http://127.0.0.1:${sp},cc.test=http://127.0.0.1:${sp}")
set(ENV{SELECTA_CACHE_DIR} "${CCACHE}")
set(ENV{SELECTA_CCMIXTER_BASE} "http://cc.test")

# 1. Sync (the Sources page Sync button sends the web site home).
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 2000 --dump ccmixter "http://ccmixter.org/"
  OUTPUT_VARIABLE so ERROR_VARIABLE se RESULT_VARIABLE src TIMEOUT 40)
if(NOT src EQUAL 0 OR NOT so MATCHES "listing_rows=2" OR NOT so MATCHES "skipped_rows=2" OR NOT so MATCHES "net_requests=1")
  message(FATAL_ERROR "I725g: want 2 rows, 2 skipped, 1 request (rc=${src})\n${so}${se}")
endif()
if(NOT so MATCHES "ccmixter_row=101/artist_one/Alpha-Mix\\|ccMixter - CC BY-NC 4.0\\|artist_one\\|"
   OR NOT so MATCHES "102/artist_two/Beta-Mix\\|ccMixter - CC NC-SAMPLING\\+ 1.0")
  message(FATAL_ERROR "I725g: rows, artists or per-upload licences are wrong\n${so}")
endif()
if(so MATCHES "Other-Licence" OR so MATCHES "Wav-Only")
  message(FATAL_ERROR "I725g: an upload without an open licence or without an MP3 was added\n${so}")
endif()
if(NOT so MATCHES "https://ccmixter.org/files/artist_one/101")
  message(FATAL_ERROR "I725g: the row has no upload-page backlink\n${so}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "cc_query=1")
  message(FATAL_ERROR "I725g: the fixture saw no query request\n${rq}")
endif()

# 2. Descriptive User-Agent on every request.
string(REGEX MATCHALL "cc_ua=" ua_all "${rq}")
string(REGEX MATCHALL "cc_ua=selecta-ccmixter/1 " ua_ok "${rq}")
list(LENGTH ua_all n_all)
list(LENGTH ua_ok n_ok)
if(n_all EQUAL 0 OR NOT n_all EQUAL n_ok)
  message(FATAL_ERROR "I725g: ${n_all} requests, only ${n_ok} with the descriptive User-Agent")
endif()

# 3. Play.
file(WRITE "${WORK}/cc.sel"
  "@dump ${WORK}/cc.raw\nOPEN ccmixter:102/artist_two/Beta-Mix\nPLAY\n@render 22050\n@sleep 600\n@render 22050\n@stream_source\n@stream_live\nSTOP\n")
execute_process(COMMAND "${CLI}" script "${WORK}/cc.sel" --db "${DB}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 40)
if(NOT rc EQUAL 0 OR o MATCHES "ERROR" OR e MATCHES "ERROR")
  message(FATAL_ERROR "I725g: script failed rc=${rc}\n${o}${e}")
endif()
if(NOT o MATCHES "STREAMSOURCE resolves=[1-9]")
  message(FATAL_ERROR "I725g: the track was not resolved through the source\n${o}")
endif()
if(NOT o MATCHES "STREAMLIVE opens=[1-9]")
  message(FATAL_ERROR "I725g: the resolved stream did not open through sel_live\n${o}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "cc_ids=102" OR NOT rq MATCHES "icy=1")
  message(FATAL_ERROR "I725g: no id lookup or no stream request\n${rq}")
endif()
file(READ "${WORK}/cc.raw" hex HEX)
if(NOT hex MATCHES "[1-9a-f]")
  message(FATAL_ERROR "I725g: the render is silent")
endif()

# 4. No file left in the cache dir.
file(GLOB_RECURSE left "${CCACHE}/*")
if(left)
  message(FATAL_ERROR "I725g: files left in the cache dir: ${left}")
endif()
message(STATUS "ccmixter_ok")
