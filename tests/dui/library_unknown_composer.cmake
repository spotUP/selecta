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

# The group opens its composer page like any composer: through its tile on the Browse page (I678i: the Library page is retired).
unk_run(browse "wait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\ntopbar 2\nwait 3\n")
dui_browse_composer_input(open_input "wait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\n" "${browse}")
unk_run(drilled "${open_input}")
if(NOT "${drilled}" MATCHES "collection kind=composer [^\n]* of=3 ")
  message(FATAL_ERROR "the Unknown composer tile did not open its composer page with 3 songs:\n${drilled}")
endif()
