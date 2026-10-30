# I725e: the Jamendo source, driven through selecta-cli against a local fixture (recorded-style API answer, no network).
#  1. no client id: `library sync jamendo` fails with the "no client id" line and adds nothing (source disabled, no crash);
#  2. with a client id: the sync reads the fixture's /v3.0/tracks answer: 2 rows, 1 skipped (no audio), net_requests=1
#     (sentinel that the plugin ran), artist, licence in the format column, and the track-page backlink as the row url;
#  3. OPEN jamendo:<id>/.. + PLAY asks /tracks?id=<id> for the stream URL (fixture log jam_play=<id>; sentinel
#     STREAMSOURCE resolves>=1) and plays it through sel_live (STREAMLIVE opens>=1); the render is not silent;
#  4. nothing was written to the cache dir: terms "must not be specifically designed to cache the content".
# -DCLI=<selecta-cli> -DWORK=<scratch dir> -DMP3=<tests/m3/stream/sine.mp3> -DICY=<icy_server.py>
set(PORT "${WORK}/jam-port")
set(DB "${WORK}/jam.db")
set(JCACHE "${WORK}/jam-cache")
file(REMOVE_RECURSE "${PORT}" "${PORT}.requests" "${DB}" "${WORK}/jam.raw" "${JCACHE}")
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
  message(FATAL_ERROR "I725e: fixture server did not start")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "icy.test=http://127.0.0.1:${sp},jam.test=http://127.0.0.1:${sp}")
set(ENV{SELECTA_CACHE_DIR} "${JCACHE}")
set(ENV{SELECTA_JAMENDO_API} "http://jam.test/v3.0")

# 1. No client id.
unset(ENV{SELECTA_JAMENDO_CLIENT_ID})
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 1000 --dump jamendo "https://www.jamendo.com/"
  OUTPUT_VARIABLE no ERROR_VARIABLE ne RESULT_VARIABLE nrc TIMEOUT 30)
if(nrc GREATER 1 OR nrc MATCHES "[^0-9]" OR NOT ne MATCHES "jamendo: no client id" OR no MATCHES "jamendo_row=")
  message(FATAL_ERROR "I725e: a missing client id must disable the source with a clear line (rc=${nrc})\n${no}${ne}")
endif()
if(EXISTS "${PORT}.requests")
  message(FATAL_ERROR "I725e: the source hit the network without a client id")
endif()

# 2. Sync (the Sources page Sync button sends the web site home).
set(ENV{SELECTA_JAMENDO_CLIENT_ID} "fixtureid1")
execute_process(COMMAND "${CLI}" library sync --db "${DB}" --now 2000 --dump jamendo "https://www.jamendo.com/"
  OUTPUT_VARIABLE so ERROR_VARIABLE se RESULT_VARIABLE src TIMEOUT 30)
if(NOT src EQUAL 0 OR NOT so MATCHES "listing_rows=2" OR NOT so MATCHES "skipped_rows=1" OR NOT so MATCHES "net_requests=1")
  message(FATAL_ERROR "I725e: want 2 track rows, 1 skipped (no audio), 1 request (rc=${src})\n${so}${se}")
endif()
if(NOT so MATCHES "jamendo_row=1001/Artist-One/Alpha-Song\\|Jamendo - CC BY-NC-SA 3.0\\|Artist-One\\|Alpha-Song\\|" OR NOT so MATCHES "1002/Artist-Two/Beta-Song")
  message(FATAL_ERROR "I725e: artist, title and licence are not in the rows\n${so}")
endif()
if(NOT so MATCHES "https://www.jamendo.com/track/1001")
  message(FATAL_ERROR "I725e: the row has no track-page backlink\n${so}")
endif()
if(so MATCHES "1003")
  message(FATAL_ERROR "I725e: the track without audio was added\n${so}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "jam_tracks=1" OR NOT rq MATCHES "jam_client=fixtureid1")
  message(FATAL_ERROR "I725e: the fixture saw no /tracks request with the client id\n${rq}")
endif()

# 3. Play.
file(WRITE "${WORK}/jam.sel"
  "@dump ${WORK}/jam.raw\nOPEN jamendo:1002/Artist-Two/Beta-Song\nPLAY\n@render 22050\n@sleep 600\n@render 22050\n@stream_source\n@stream_live\nSTOP\n")
execute_process(COMMAND "${CLI}" script "${WORK}/jam.sel" --db "${DB}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 25)
if(NOT rc EQUAL 0 OR o MATCHES "ERROR" OR e MATCHES "ERROR")
  message(FATAL_ERROR "I725e: script failed rc=${rc}\n${o}${e}")
endif()
if(NOT o MATCHES "STREAMSOURCE resolves=[1-9]")
  message(FATAL_ERROR "I725e: the track was not resolved through the source\n${o}")
endif()
if(NOT o MATCHES "STREAMLIVE opens=[1-9]")
  message(FATAL_ERROR "I725e: the resolved stream did not open through sel_live\n${o}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "jam_play=1002" OR NOT rq MATCHES "icy=1")
  message(FATAL_ERROR "I725e: no id lookup for the played track, or no stream request\n${rq}")
endif()
file(READ "${WORK}/jam.raw" hex HEX)
if(NOT hex MATCHES "[1-9a-f]")
  message(FATAL_ERROR "I725e: the render is silent")
endif()

# 4. No file left in the cache dir after sync and play (audio is never copied).
file(GLOB_RECURSE left "${JCACHE}/*")
if(left)
  message(FATAL_ERROR "I725e: files left in the cache dir: ${left}")
endif()
message(STATUS "jamendo_ok")
