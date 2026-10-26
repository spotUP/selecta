# dui-library-unknown-composer (DUI-P3-15, I353): the Composers tab groups songs without a composer under "Unknown composer".
#   -DSELECTA -DSONGS -DWORK
#   Violated invariant: every song is reachable from the Composers tab. The library's composer list (SEL_COMPOSER_LIST) emits one
#   group for the songs with no song_composers row, labelled SEL_COMPOSER_UNKNOWN_DISPLAY, sorted last; the page paints it like
#   any row. Three copies of the test mod (no composer tags) are scanned with `exec LIBRARY_SCAN` into an empty db: the dump holds
#   exactly one composer row, "Unknown composer" with 3 songs, and a click on it drills in to the 3 songs.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()
set(db "${WORK}/library.db")

function(unk_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 900x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

unk_run(list "wait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\n")
if(NOT "${list}" MATCHES "\ncomposer 0 [^|\n]*\\|Unknown composer\\|3\n")
  message(FATAL_ERROR "no 'Unknown composer' group with 3 songs as composer 0 in:\n${list}")
endif()
if("${list}" MATCHES "\ncomposer 1 ")
  message(FATAL_ERROR "more than one composer group for untagged songs:\n${list}")
endif()

# The group is a row like any other: a click on it drills in to its 3 songs.
if(NOT "${list}" MATCHES "row 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row rectangle in the library dump:\n${list}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + 10")
math(EXPR cy "${CMAKE_MATCH_2} + 8")
unk_run(drilled "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 1\n")
if(NOT "${drilled}" MATCHES "drill Unknown composer\\|3\n")
  message(FATAL_ERROR "click on the Unknown composer row did not drill in to 3 songs:\n${drilled}")
endif()
