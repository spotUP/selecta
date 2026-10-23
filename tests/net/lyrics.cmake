# T93 lyrics (row I193): `selecta-cli script tests/m3/lyrics.sel` over the devilbox fixture.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DM3=<tests/m3>
# Songs and lyrics: tests/m3/lyrics/ (message.it is an Impulse Tracker module with no samples and one empty pattern,
# carrying a three line song message). Precedence: .lrc next to the song, song message, DEViLBOX endpoint.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=${fx},fixture.test=${fx}")
set(ENV{SELECTA_DEVILBOX_URL} "http://devilbox.uprough.net")

function(reset_log)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
endfunction()

# Number of requests on /api/lyrics/ in the fixture log.
function(lyrics_requests var)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/t93-log.json" OUTPUT_VARIABLE o)
  file(READ "${WORK}/t93-log.json" logjson)
  string(JSON n LENGTH "${logjson}")
  set(c 0)
  if(n GREATER 0)
    math(EXPR last "${n} - 1")
    foreach(i RANGE ${last})
      string(JSON path GET "${logjson}" ${i} path)
      if(path MATCHES "^/api/lyrics/")
        math(EXPR c "${c} + 1")
      endif()
    endforeach()
  endif()
  set(${var} ${c} PARENT_SCOPE)
endfunction()

function(expect_match text pattern what)
  if(NOT text MATCHES "${pattern}")
    message(FATAL_ERROR "T93 lyrics: ${what}: want /${pattern}/\n${text}")
  endif()
endfunction()

function(run_script out script db)
  file(REMOVE "${db}" "${db}-wal" "${db}-shm")
  file(REMOVE_RECURSE "${WORK}/t93-config")
  file(MAKE_DIRECTORY "${WORK}/t93-config")
  execute_process(COMMAND "${CLI}" script "${M3}/${script}" --db "${db}" --config-dir "${WORK}/t93-config"
    WORKING_DIRECTORY "${M3}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "T93 lyrics: ${script} failed rc=${rc}\n${o}${e}")
  endif()
  set(${out} "${o}${e}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK}/t93-config")
file(MAKE_DIRECTORY "${WORK}/t93-config")

# ---- 1. all four songs in one script
reset_log()
run_script(r "lyrics.sel" "${WORK}/t93-1.db")
string(REGEX MATCHALL "LYRICS [^\n]*" lines "${r}")
list(LENGTH lines nlines)
if(NOT nlines EQUAL 5)
  message(FATAL_ERROR "T93 lyrics: ${nlines} LYRICS lines, want 5\n${r}")
endif()
list(GET lines 0 l_lrc)
list(GET lines 1 l_msg)
list(GET lines 2 l_dvb)
list(GET lines 3 l_dvb_seek)
list(GET lines 4 l_missing)
# local file: at 12.5 s the current line is the 00:12.00 one, index 3 of 6 (the golden); no request was sent
expect_match("${l_lrc}" "lyric_source=lrc lyric_line=3 lyric_lines=6 lyric_fetches=0 text=Line three, sung at twelve seconds" "lrc at 12.5 s")
# song message: plain lyrics, no synced lines, still no request
expect_match("${l_msg}" "lyric_source=message lyric_line=-1 lyric_lines=0 lyric_fetches=0 text=First line of the song message\\|Second line sings along\\|Third line fades out"  "song message")
expect_match("${l_msg}" "Third line fades out" "whole song message kept")
# endpoint: used because there is no local file; no line has started at 0 s
expect_match("${l_dvb}" "lyric_source=devilbox lyric_line=-1 lyric_lines=3 lyric_fetches=1" "endpoint lyrics before the first line")
expect_match("${l_dvb_seek}" "lyric_source=devilbox lyric_line=1 lyric_lines=3 lyric_fetches=1 text=Endpoint line two" "endpoint lyrics at 2.6 s")
# missing route: the song plays without lyrics and the sentinel names the endpoint
expect_match("${r}" "devilbox_missing=lyrics\nLYRICS lyric_source=none lyric_line=-1 lyric_lines=0 lyric_fetches=2" "missing endpoint")
lyrics_requests(n1)
if(NOT n1 EQUAL 2)
  message(FATAL_ERROR "T93 lyrics: ${n1} /api/lyrics requests, want 2 (techno, nogenre; none for lrc and message)")
endif()

# ---- 2. the pref off: no request at all, no lyrics
reset_log()
run_script(r2 "lyrics_off.sel" "${WORK}/t93-2.db")
expect_match("${r2}" "LYRICS lyric_source=none lyric_line=-1 lyric_lines=0 lyric_fetches=0" "pref off")
lyrics_requests(n2)
if(NOT n2 EQUAL 0)
  message(FATAL_ERROR "T93 lyrics: ${n2} /api/lyrics requests with library.lyrics_online off, want 0")
endif()

message(STATUS "lyrics_ok")
