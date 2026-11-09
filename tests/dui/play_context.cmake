# dui-play-context (I994e): a click on a collection-page row sends PLAY_CONTEXT collection (design 2026-11-02-I994 section 6, D5): the engine
# queues the whole page, plays the clicked row, names the context after the playlist. -DSELECTA -DSONGS -DWORK
# Dump line `playctx plays= rows= ctx=[] queue= heading=[]`: plays/rows are the engine context_plays/context_rows sentinels.
# 1. alpha (3 songs) opened, row 1 (Intro, of 3) clicked: plays=1 rows=2 ctx=[alpha] queue=2 (rows from the clicked one on, design section 3) and the Now Playing heading reads alpha.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-collection-page needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()

# The fixture keys are fixture:N; the title and cover lookups go by the local:<path> key a played file has.
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%'"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "rekeying the fixture failed: ${err}")
endif()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()
# The centre of the sidebar row called `name` into ${out}.
function(side_row out dump name)
  if(NOT "${dump}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=[0-9]+ ${name}\n")
    message(FATAL_ERROR "no sidebar row '${name}' in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()
# One field of the collection line.
function(col_field out dump field)
  if(NOT "${dump}" MATCHES "collection kind=playlist [^\n]*${field}=([^ \n]*)")
    message(FATAL_ERROR "no collection line with ${field} in:\n${dump}")
  endif()
  set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
function(titles_of out dump)
  string(REGEX MATCHALL "plsong [0-9]+ [0-9]+ [0-9]+ [0-9]+ [0-9]+ file=[^ \n]* title=[^\n]*" lines "${dump}")
  set(names "")
  foreach(l IN LISTS lines)
    string(REGEX MATCH "title=(.*)" m "${l}")
    list(APPEND names "${CMAKE_MATCH_1}")
  endforeach()
  set(${out} "${names}" PARENT_SCOPE)
endfunction()
# The centre of a `name=x,y,w,h` rectangle of the collection line.
function(col_rect out dump name)
  if(NOT "${dump}" MATCHES "collection kind=playlist [^\n]* ${name}=([0-9]+),([0-9]+),([0-9]+),([0-9]+)")
    message(FATAL_ERROR "no ${name} rectangle in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()

# Each open touches Recents and reorders the sidebar, so the row of `name` is read from an idle run right before the run that clicks it.
# `script` is the input after the click, `out` the dump.
function(open_run out name script)
  dui_run(idle_${name} ${common} --input "${WORK}/idle.txt")
  side_row(xy "${idle_${name}}" "${name}")
  file(WRITE "${WORK}/${name}.txt" "click ${xy}\nwait 100\n${script}")
  dui_run(run_${name} ${common} --input "${WORK}/${name}.txt")
  set(${out} "${run_${name}}" PARENT_SCOPE)
endfunction()

set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x800)
dui_run(seed ${common}
  --exec "PLAYLIST_CREATE alpha" --exec "PLAYLIST_ADD_SONG 1 fixture/2.mod" --exec "PLAYLIST_ADD_SONG 1 fixture/1.mod" --exec "PLAYLIST_ADD_SONG 1 fixture/5.sid"
  --exec "PLAYLIST_CREATE beta" --exec "PLAYLIST_ADD_SONG 2 fixture/1.mod"
  --exec "PLAYLIST_CREATE gamma" --exec "PLAYLIST_ADD_SONG 3 fixture/4.it")
file(WRITE "${WORK}/idle.txt" "wait 100\n")


file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(idle ${common} --input "${WORK}/idle.txt")
side_row(xy "${idle}" "alpha")
file(WRITE "${WORK}/open.txt" "click ${xy}\nwait 200\n")
dui_run(opened ${common} --input "${WORK}/open.txt")
if(NOT "${opened}" MATCHES "plsong 1 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no second row on the alpha page:\n${opened}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + 12") # the row centre can sit on the artist or album link
math(EXPR ry "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
# Opening touches Recents and reorders the sidebar, so the row of alpha is read again right before the run that clicks it.
dui_run(idle2 ${common} --input "${WORK}/idle.txt")
side_row(xy2 "${idle2}" "alpha")
file(WRITE "${WORK}/play.txt" "click ${xy2}\nwait 200\nclick ${rx} ${ry}\nwait 400\n")
dui_run(played ${common} --input "${WORK}/play.txt")
if(NOT "${played}" MATCHES "playctx plays=([0-9]+) rows=([0-9]+) ctx=\\[([^]]*)\\] queue=([0-9]+) heading=\\[([^]]*)\\]")
  message(FATAL_ERROR "no playctx line:\n${played}")
endif()
expect_eq("PLAY_CONTEXT sent once" "${CMAKE_MATCH_1}" "1")
expect_eq("context rows added besides the clicked song" "${CMAKE_MATCH_2}" "2")
expect_eq("engine context name" "${CMAKE_MATCH_3}" "alpha")
expect_eq("queue holds the clicked row and the rows after it" "${CMAKE_MATCH_4}" "2")
expect_eq("Now Playing heading" "${CMAKE_MATCH_5}" "alpha")
message(STATUS "play_context_ok")
