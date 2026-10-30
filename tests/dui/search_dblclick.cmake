# dui-search-dblclick (I714): a double-click on a Search result song row plays that song, through the real shell and the scripted
# --input feeder (`dclick x y`). -DSELECTA -DSONGS -DWORK
# A scanned library holds one song, banana.mod. Typing `/banana` Enter lists it under Songs; the dump line `searchrow` is the
# registered song row's rectangle. A single click must not play it (engine song stays empty, row_plays 0); the double-click
# plays it through desk_play_path: the engine's `song` path is the row's file, row_plays 1 and double_clicks 1 (sentinels that
# song_row_dblclick ran). The Songs page double-click is covered by dui-mouse; the lists that already play on the first click
# (playlist, kind pages, Library, Home tiles) are not routed through this handler.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
configure_file("${SONGS}/selecta-test.mod" "${WORK}/songs/banana.mod" COPYONLY)

function(sd_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(REMOVE "${WORK}/library.db")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
    --exec "LIBRARY_SCAN ${WORK}/songs" --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

set(typed "wait 5\nkey /\nkey b\nkey a\nkey n\nkey a\nkey n\nkey a\nkey enter\nwait 600\nexec VIEW_CURSOR +0\nwait 5\n")
sd_run(base "${typed}")
if(NOT "${base}" MATCHES "searchrow 0 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) key=([^\n]*banana.mod)\n")
  message(FATAL_ERROR "no searchrow for banana.mod in:\n${base}")
endif()
math(EXPR x "${CMAKE_MATCH_1} + 40")
math(EXPR y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
string(REGEX REPLACE "^local:" "" row_key "${CMAKE_MATCH_5}")

sd_run(single "${typed}move ${x} ${y}\nclick ${x} ${y}\nwait 20\n")
dui_stat(sp "${single}" desktop row_plays)
if(NOT sp EQUAL 0 OR single MATCHES "song=[^\n]*banana.mod")
  message(FATAL_ERROR "a single click on a Search song row must not play it (row_plays ${sp}):\n${single}")
endif()

sd_run(dbl "${typed}move ${x} ${y}\ndclick ${x} ${y}\nwait 20\n")
if(NOT "${dbl}" MATCHES "song=([^\n]*)\n" OR NOT CMAKE_MATCH_1 STREQUAL "${row_key}")
  message(FATAL_ERROR "double-click on the Search row must open ${row_key}, got '${CMAKE_MATCH_1}':\n${dbl}")
endif()
dui_stat(dc "${dbl}" desktop double_clicks)
dui_stat(rp "${dbl}" desktop row_plays)
dui_stat(ps "${dbl}" desktop play_state)
if(NOT dc EQUAL 1 OR NOT rp EQUAL 1 OR NOT ps EQUAL 1)
  message(FATAL_ERROR "double_clicks=${dc} row_plays=${rp} play_state=${ps}, expected 1 1 1")
endif()
