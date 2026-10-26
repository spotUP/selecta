# dui-song-menu (DUI-P3-16, I305): one song menu serves every page that shows a song row.
#   -DSELECTA -DSONGS -DWORK
#   Violated invariant: a song row anywhere offers Play, Play Next, Add to Queue and Add to Playlist. Rows register with the one
#   shared registry while they draw (song_row_reg); right-click opens the one menu (song_menu_open). Three songs are scanned,
#   the Composers drilldown shows them, a right-click on song row 2 opens the menu with its 4 items (no Remove: not a queue
#   row), Add to Queue grows the engine queue by 1 and the sentinel counts the open and the pick.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()
set(db "${WORK}/library.db")

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got ${got}, want ${want}")
  endif()
endfunction()

function(sm_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 900x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(sm_field outvar out name)
  if(NOT "${out}" MATCHES "song_menu [^\n]*${name}=([0-9]+)")
    message(FATAL_ERROR "no song_menu ${name} in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

set(scan "wait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\n")
sm_run(list "${scan}")
if(NOT "${list}" MATCHES "row 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row rectangle in the library dump:\n${list}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + 10")
math(EXPR cy "${CMAKE_MATCH_2} + 8")
set(drill "${scan}move ${cx} ${cy}\nclick ${cx} ${cy}\nwait 1\n")
sm_run(drilled "${drill}")
# Drilldown song rows sit below the list top at one row height each (lib_row_rect); song i is list row i + 1, so row 2 of the
# list is the second song.
if(NOT "${list}" MATCHES "row 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row rectangle in the library dump:\n${list}")
endif()
math(EXPR sx "${CMAKE_MATCH_1} + 20")
math(EXPR sy "${CMAKE_MATCH_2} + 2 * ${CMAKE_MATCH_4} + ${CMAKE_MATCH_4} / 2")
sm_field(opens0 "${drilled}" opens)
sm_field(rows "${drilled}" rows)
if(rows LESS 3)
  message(FATAL_ERROR "the drilldown registered ${rows} song rows, expected 3:\n${drilled}")
endif()

# Right-click on row 2: the menu is open with its 4 items.
sm_run(opened "${drill}move ${sx} ${sy}\nrclick ${sx} ${sy}\nwait 1\n")
sm_field(open1 "${opened}" open)
sm_field(items "${opened}" items)
sm_field(opens1 "${opened}" opens)
if(NOT open1 EQUAL 1 OR NOT items EQUAL 5 OR NOT opens1 EQUAL 1)
  message(FATAL_ERROR "right-click opened open=${open1} items=${items} opens=${opens1}, expected 1/5/1:\n${opened}")
endif()

# Add to Queue is the third item: the engine queue grows by 1.
math(EXPR qx "${sx} + 10")
math(EXPR qy "${sy} + 2 * 24 + 12")
sm_run(queued "${drill}move ${sx} ${sy}\nrclick ${sx} ${sy}\nclick ${qx} ${qy}\nwait 1\n")
sm_field(picks "${queued}" picks)
expect_eq("song menu picks" "${picks}" 1)
if(NOT "${opened}" MATCHES "playlist_entries=([0-9]+)")
  message(FATAL_ERROR "no playlist_entries in:\n${opened}")
endif()
set(q0 "${CMAKE_MATCH_1}")
if(NOT "${queued}" MATCHES "playlist_entries=([0-9]+)")
  message(FATAL_ERROR "no playlist_entries in:\n${queued}")
endif()
math(EXPR grew "${CMAKE_MATCH_1} - ${q0}")
expect_eq("engine queue growth after Add to Queue" "${grew}" 1)
