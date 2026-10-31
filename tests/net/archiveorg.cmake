# I725d: the Internet Archive source, driven through selecta-cli against a local fixture (recorded-style API answers).
#  1. sync reads the fixture's advancedsearch answer (5 hits). Licence gate: the hit with no licence and the hit with a
#     non-CC licence are dropped without a metadata request; a CC item with no MP3/Ogg file is skipped. Rows:
#     ia_cc_one 2 MP3 rows (the Ogg twin is ignored), ia_pd 1 row (title from the file name) = listing_rows=3,
#     skipped_rows=3, net_requests=4 (1 search + 3 metadata; the 429 retry is not counted: sentinel that the plugin ran);
#  2. the first metadata answer is 429 + Retry-After: 1; the plugin waits and retries (two ia_meta=ia_cc_one hits, the
#     second at least 1 s after the 429);
#  3. every request carries the descriptive User-Agent (Selecta-ArchiveOrg/1 ...);
#  4. OPEN archiveorg:<id>/.. + PLAY asks /metadata/<id> for the file, streams /download/<id>/<file> through sel_live;
#  5. nothing is left in the cache dir (audio and JSON are never kept).
# -DCLI=<selecta-cli> -DWORK=<scratch dir> -DMP3=<tests/m3/stream/sine.mp3> -DICY=<icy_server.py>
set(PORT "${WORK}/ia-port")
set(DB "${WORK}/ia.db")
set(ICACHE "${WORK}/ia-cache")
file(REMOVE_RECURSE "${PORT}" "${PORT}.requests" "${DB}" "${WORK}/ia.raw" "${ICACHE}")
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
  message(FATAL_ERROR "I725d: fixture server did not start")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "icy.test=http://127.0.0.1:${sp},ia.test=http://127.0.0.1:${sp}")
set(ENV{SELECTA_CACHE_DIR} "${ICACHE}")
set(ENV{SELECTA_ARCHIVEORG_BASE} "http://ia.test")

# 1. Sync (the Sources page Sync button sends the web site home).
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 2000 --dump archiveorg "https://archive.org/"
  OUTPUT_VARIABLE so ERROR_VARIABLE se RESULT_VARIABLE src TIMEOUT 40)
if(NOT src EQUAL 0 OR NOT so MATCHES "listing_rows=3" OR NOT so MATCHES "skipped_rows=3" OR NOT so MATCHES "net_requests=4")
  message(FATAL_ERROR "I725d: want 3 rows, 3 skipped, 4 requests (rc=${src})\n${so}${se}")
endif()
if(NOT so MATCHES "archiveorg_row=ia_cc_one/Artist-One/Alpha-Song\\|Internet Archive - CC BY 4.0\\|Artist-One\\|Alpha-Song\\|"
   OR NOT so MATCHES "ia_cc_one/Artist-One/Beta-Song" OR NOT so MATCHES "ia_pd/Artist-Two/tune_one\\|Internet Archive - CC0 1.0")
  message(FATAL_ERROR "I725d: rows, artists or licences are wrong\n${so}")
endif()
if(so MATCHES "ia_none" OR so MATCHES "ia_other" OR so MATCHES "ia_nomp3")
  message(FATAL_ERROR "I725d: an item without an open licence or without MP3/Ogg was added\n${so}")
endif()
if(NOT so MATCHES "https://archive.org/details/ia_cc_one")
  message(FATAL_ERROR "I725d: the row has no item-page backlink\n${so}")
endif()
file(READ "${PORT}.requests" rq)
if(rq MATCHES "ia_meta=ia_none" OR rq MATCHES "ia_meta=ia_other")
  message(FATAL_ERROR "I725d: metadata was requested for an item without an open licence\n${rq}")
endif()
if(NOT rq MATCHES "ia_search=1" OR NOT rq MATCHES "ia_meta=ia_pd")
  message(FATAL_ERROR "I725d: the fixture saw no search or metadata request\n${rq}")
endif()

# 2. 429 + Retry-After: 1 then a retry at least one second later.
string(REGEX MATCH "ia_429=([0-9.]+)" m429 "${rq}")
set(t429 "${CMAKE_MATCH_1}")
string(REGEX MATCH "ia_meta_ok=ia_cc_one ([0-9.]+)" mok "${rq}")
set(tok "${CMAKE_MATCH_1}")
if(t429 STREQUAL "" OR tok STREQUAL "")
  message(FATAL_ERROR "I725d: no 429 served, or no retry answered 200\n${rq}")
endif()
execute_process(COMMAND python3 -c "import sys; sys.exit(0 if float(sys.argv[2]) - float(sys.argv[1]) >= 1.0 else 1)" "${t429}" "${tok}"
  RESULT_VARIABLE wrc)
if(NOT wrc EQUAL 0)
  message(FATAL_ERROR "I725d: the retry came less than Retry-After seconds after the 429 (${t429} -> ${tok})")
endif()

# 3. Descriptive User-Agent on every request.
string(REGEX MATCHALL "ia_ua=" ua_all "${rq}")
string(REGEX MATCHALL "ia_ua=selecta-archiveorg/1 " ua_ok "${rq}")
list(LENGTH ua_all n_all)
list(LENGTH ua_ok n_ok)
if(n_all EQUAL 0 OR NOT n_all EQUAL n_ok)
  message(FATAL_ERROR "I725d: ${n_all} requests, only ${n_ok} with the descriptive User-Agent")
endif()

# 4. Play.
file(WRITE "${WORK}/ia.sel"
  "@dump ${WORK}/ia.raw\nOPEN archiveorg:ia_cc_one/Artist-One/Beta-Song\nPLAY\n@render 22050\n@sleep 600\n@render 22050\n@stream_source\n@stream_live\nSTOP\n")
execute_process(COMMAND "${CLI}" script "${WORK}/ia.sel" --db "${DB}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 40)
if(NOT rc EQUAL 0 OR o MATCHES "ERROR" OR e MATCHES "ERROR")
  message(FATAL_ERROR "I725d: script failed rc=${rc}\n${o}${e}")
endif()
if(NOT o MATCHES "STREAMSOURCE resolves=[1-9]")
  message(FATAL_ERROR "I725d: the track was not resolved through the source\n${o}")
endif()
if(NOT o MATCHES "STREAMLIVE opens=[1-9]")
  message(FATAL_ERROR "I725d: the resolved stream did not open through sel_live\n${o}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "icy=1")
  message(FATAL_ERROR "I725d: no stream request after the metadata lookup\n${rq}")
endif()
file(READ "${WORK}/ia.raw" hex HEX)
if(NOT hex MATCHES "[1-9a-f]")
  message(FATAL_ERROR "I725d: the render is silent")
endif()

# 5. No file left in the cache dir.
file(GLOB_RECURSE left "${ICACHE}/*")
if(left)
  message(FATAL_ERROR "I725d: files left in the cache dir: ${left}")
endif()
message(STATUS "archiveorg_ok")
