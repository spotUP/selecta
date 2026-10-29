# dui-drop (DUI-P5-11, I276 + I275): adding music on the desktop. -DSELECTA -DSONGS -DWORK
# 1. The Library page draws an Add Folder button (the `addfolder x y w h` dump line). A click on it with
#    --dialog-answer <folder> runs LIBRARY_SCAN over the folder: the scan sentinel counts one scan, ui_drop counts one drop,
#    and the Tree tab lists the three scanned songs under their format node.
# 2. One dropped file plays: the engine opens it (songs_opened=1) and the queue stays empty.
# 3. Two dropped files queue: the queue view lists two entries and nothing is opened.
# 4. A dropped path that does not exist raises a toast and is still counted.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()

function(drop_run outvar view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(REMOVE "${WORK}/library.db")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --start-dir "${WORK}/songs" --db "${WORK}/library.db" --ui desktop --backend mem --view ${view} --canvas 900x600
    --clock fixed --stats --dialog-answer "${WORK}/songs" --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', want '${want}'")
  endif()
endfunction()

# 1. Add Folder.
drop_run(idle library "wait 1\n")
if(NOT "${idle}" MATCHES "addfolder ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no addfolder line in the Library dump:\n${idle}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
math(EXPR ax1 "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3}")
math(EXPR ay0 "${CMAKE_MATCH_2}")
math(EXPR ay1 "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4}")
# the button ends left of the history arrows, which own the top right corner (I284): no overlap with either
if(NOT "${idle}" MATCHES "\nnav_arrows (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no nav_arrows line in the Library dump:\n${idle}")
endif()
math(EXPR by1 "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4}")
if(NOT ax1 LESS CMAKE_MATCH_1 AND ay0 LESS by1 AND CMAKE_MATCH_2 LESS ay1)
  message(FATAL_ERROR "Add Folder (right ${ax1}, y ${ay0}..${ay1}) overlaps the back arrow at ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}")
endif()
if(NOT "${idle}" MATCHES "scans 0")
  message(FATAL_ERROR "scan sentinel not zero before the click:\n${idle}")
endif()
drop_run(added library "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 5\n")
if(NOT "${added}" MATCHES "scan probed [1-9][0-9]* files 3 scans 1")
  message(FATAL_ERROR "Add Folder did not scan the folder once:\n${added}")
endif()
if(NOT "${added}" MATCHES "ui_drop id=desktop drops=1 scans=1")
  message(FATAL_ERROR "ui_drop sentinel wrong after Add Folder:\n${added}")
endif()

# 2. One file plays.
drop_run(one queue "wait 1\ndrop ${WORK}/songs/a.mod\nwait 20\n")
if(NOT "${one}" MATCHES "songs_opened=1")
  message(FATAL_ERROR "a dropped file was not opened:\n${one}")
endif()
if(NOT "${one}" MATCHES "queue draws [0-9]+ rows [0-9]+ cmds [0-9]+ hash [0-9a-f]+ count 0 ")
  message(FATAL_ERROR "a single drop must not queue:\n${one}")
endif()
if(NOT "${one}" MATCHES "ui_drop id=desktop drops=1 scans=0")
  message(FATAL_ERROR "ui_drop sentinel wrong after one file:\n${one}")
endif()

# 3. Two files queue.
drop_run(two queue "wait 1\ndrop ${WORK}/songs/a.mod ${WORK}/songs/b.mod\nwait 20\n")
if(NOT "${two}" MATCHES "queue draws [0-9]+ rows [0-9]+ cmds [0-9]+ hash [0-9a-f]+ count 2 ")
  message(FATAL_ERROR "two dropped files did not queue two entries:\n${two}")
endif()
if(NOT "${two}" MATCHES "ui_drop id=desktop drops=2 scans=0")
  message(FATAL_ERROR "ui_drop sentinel wrong after two files:\n${two}")
endif()
if("${two}" MATCHES "songs_opened=[1-9]")
  message(FATAL_ERROR "queued drops must not open a song:\n${two}")
endif()

# 4. Missing path.
drop_run(bad queue "wait 1\ndrop ${WORK}/songs/missing.mod\nwait 5\n")
if(NOT "${bad}" MATCHES "toasts=[1-9]")
  message(FATAL_ERROR "a missing dropped path raised no toast:\n${bad}")
endif()
if(NOT "${bad}" MATCHES "ui_drop id=desktop drops=1 ")
  message(FATAL_ERROR "the missing drop was not counted:\n${bad}")
endif()
# 5. A finished scan reloads the page it ends on (I296): Home and the Library page in Composers mode change their drawn rows
#    without a page change. The fixture is four real tagged files; Home's "Recently played" holds one of them by path before
#    the scan (a play from a folder the library did not know yet), so the shelf tile is named by path until the scan has
#    the song's title and artist.
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-drop needs the sqlite3 command line tool to seed the play history")
endif()
file(MAKE_DIRECTORY "${WORK}/tagged")
foreach(n tagged_v23.mp3 tagged_v24.mp3 tagged_v1.mp3 tagged.ogg)
  file(COPY_FILE "${TAGGED}/${n}" "${WORK}/tagged/${n}")
endforeach()
set(rdb "${WORK}/reload.db")
function(reload_run outvar view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(REMOVE "${rdb}")
  dui_run(init --db "${rdb}" --ui desktop --backend mem --view ${view} --canvas 900x600 --clock fixed --stats)
  execute_process(COMMAND "${SQLITE3}" "${rdb}"
    "PRAGMA trusted_schema = 1; INSERT INTO history(key, played_at, skipped) VALUES('local:${WORK}/tagged/tagged_v23.mp3', 1000, 0)"
    RESULT_VARIABLE rc ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "seeding the history failed: ${err}")
  endif()
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${rdb}" --ui desktop --backend mem --view ${view} --canvas 900x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

reload_run(home_before home "wait 3\n")
reload_run(home_after home "wait 1\ndrop ${WORK}/tagged\nwait 20\n")
if(NOT "${home_after}" MATCHES "ui_drop id=desktop drops=1 scans=1")
  message(FATAL_ERROR "the drop did not scan once on Home:\n${home_after}")
endif()
string(REGEX MATCHALL "shelf 0 tile [0-9]+ [^\n]*" home_before_tiles "${home_before}")
string(REGEX MATCHALL "shelf 0 tile [0-9]+ [^\n]*" home_after_tiles "${home_after}")
# The seeded play is of a local: song that is not in the library before the drop, so Home hides it (I555); the
# scan puts the song in the library and Home must reload and show the row.
if("${home_after_tiles}" STREQUAL "" OR "${home_before_tiles}" STREQUAL "${home_after_tiles}")
  message(FATAL_ERROR "Home kept its rows after the scan:\nbefore ${home_before_tiles}\nafter ${home_after_tiles}")
endif()
if(NOT "${home_after}" MATCHES "nav page=home ")
  message(FATAL_ERROR "the page changed during the scan:\n${home_after}")
endif()

# SKIPPED (I678i): the Library page Composers mode reload after a scan is retired. Composers are the sidebar chip and composer pages (section 10);
# the Home half above stays. The sidebar reload on scan has no test row yet (Home shelves: I678j).
message(STATUS "dui-drop: Library Composers reload skipped (page retired)")
message(STATUS "dui-drop ok")
