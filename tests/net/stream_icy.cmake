# I196a + I196c: ICY streams and directory recording, driven through selecta-cli (script and play).
# icy_server.py honours `Icy-MetaData: 1`: icy-metaint 512, StreamTitle "Artist - First Song" at chunk 1 and
# "Artist - Second Song" at chunk 8 (repeated at chunk 12, which must NOT split), empty blocks elsewhere, 21 chunks.
# Checks:
#  1. the request carried Icy-MetaData: 1;
#  2. OPTION record_to <directory longer than 63 characters> writes exactly two files named by the titles, the first
#     with the audio of chunks 1-8, the second with chunks 9-21 (byte-compare after the 512-byte ID3 block, so no
#     metadata byte reached the file), each tag holding its title (TIT2);
#  3. @title shows the ICY title; the decoder swallowed no metadata (the render is not silent);
#  4. a second OPEN writes "(2)" files instead of overwriting;
#  5. `play --record <dir>` (macOS, needs an output device) writes the same two files.
# -DCLI=<selecta-cli> -DWORK=<scratch dir> -DMP3=<tests/m3/stream/sine.mp3> -DICY=<icy_server.py>
set(DIR "${WORK}/icy-record-directory-with-a-deliberately-long-name-beyond-63-chars")
string(LENGTH "${DIR}" dirlen)
if(dirlen LESS 64)
  message(FATAL_ERROR "I196c: test directory is only ${dirlen} characters")
endif()
set(PORT "${WORK}/icy-port")
file(REMOVE_RECURSE "${DIR}" "${DIR}-play" "${PORT}" "${PORT}.requests")
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
  message(FATAL_ERROR "I196a: ICY server did not start")
endif()
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "icy.test=http://127.0.0.1:${sp}")

file(WRITE "${WORK}/icy.sel"
  "@dump ${WORK}/icy.raw\nOPTION record_to ${DIR}\nOPEN http://icy.test/live\nPLAY\n@render 22050\n@sleep 600\n@render 22050\n"
  "@title\n@stream_live\nOPEN http://icy.test/live\nPLAY\n@render 22050\n@sleep 600\n@render 22050\n@stream_live\nSTOP\n")
execute_process(COMMAND "${CLI}" script "${WORK}/icy.sel" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 25)
if(NOT rc EQUAL 0 OR o MATCHES "ERROR" OR e MATCHES "ERROR")
  message(FATAL_ERROR "I196a: script failed rc=${rc}\n${o}${e}")
endif()
if(NOT o MATCHES "TITLE text=Artist - Second Song")
  message(FATAL_ERROR "I196a: the ICY StreamTitle did not become the song title\n${o}")
endif()
if(NOT o MATCHES "record_files=2.*record_files=4")
  message(FATAL_ERROR "I196c: want 2 record files after the first OPEN and 4 after the second\n${o}")
endif()
file(READ "${PORT}.requests" rq)
if(NOT rq MATCHES "icy=1" OR rq MATCHES "icy=0")
  message(FATAL_ERROR "I196a: the request did not carry Icy-MetaData: 1\n${rq}")
endif()
file(READ "${WORK}/icy.raw" hex HEX)
if(NOT hex MATCHES "[1-9a-f]")
  message(FATAL_ERROR "I196a: the render is silent")
endif()

# Compares <file> (after its 512-byte tag) with the served audio segment; the tag must carry the title.
function(check_file file seg title)
  if(NOT EXISTS "${file}")
    message(FATAL_ERROR "I196c: missing recording ${file}")
  endif()
  file(READ "${file}" fh HEX)
  file(READ "${seg}" sh HEX)
  string(HEX "${title}" th)
  string(SUBSTRING "${fh}" 0 1024 tag)
  string(SUBSTRING "${fh}" 1024 -1 body)
  if(NOT tag MATCHES "^494433" OR NOT tag MATCHES "54495432" OR NOT tag MATCHES "${th}")
    message(FATAL_ERROR "I196c: ${file} has no ID3 TIT2 tag for '${title}'")
  endif()
  if(NOT body STREQUAL sh)
    string(LENGTH "${body}" bl)
    string(LENGTH "${sh}" sl)
    message(FATAL_ERROR "I196a: ${file} audio differs from the served audio without metadata (${bl} vs ${sl} hex chars)")
  endif()
endfunction()

check_file("${DIR}/Artist - First Song.mp3" "${PORT}.seg1" "Artist - First Song")
check_file("${DIR}/Artist - Second Song.mp3" "${PORT}.seg2" "Artist - Second Song")
check_file("${DIR}/Artist - First Song (2).mp3" "${PORT}.seg1" "Artist - First Song")
check_file("${DIR}/Artist - Second Song (2).mp3" "${PORT}.seg2" "Artist - Second Song")
file(GLOB all RELATIVE "${DIR}" "${DIR}/*")
list(LENGTH all n)
if(NOT n EQUAL 4)
  message(FATAL_ERROR "I196c: ${n} files in the record directory, want 4: ${all}")
endif()

# play --record: macOS with an output device only (exit 77 otherwise; the script part above already ran).
if(APPLE)
  execute_process(COMMAND "${CLI}" play http://icy.test/live --output coreaudio --seconds 2 --record "${DIR}-play"
    OUTPUT_VARIABLE po ERROR_VARIABLE pe RESULT_VARIABLE prc TIMEOUT 25)
  if(prc EQUAL 77)
    message(STATUS "I196c: no output device, play --record not run")
  elseif(NOT prc EQUAL 0)
    message(FATAL_ERROR "I196c: play --record exited ${prc}\n${po}${pe}")
  else()
    check_file("${DIR}-play/Artist - First Song.mp3" "${PORT}.seg1" "Artist - First Song")
    check_file("${DIR}-play/Artist - Second Song.mp3" "${PORT}.seg2" "Artist - Second Song")
  endif()
endif()
message(STATUS "I196a: ICY titles split ${n} recordings, no metadata bytes in the audio")
message(STATUS "stream_icy_ok")
