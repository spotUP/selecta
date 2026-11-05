# dui-library-follow-keeps-place (I1086): a library change refreshes data in place and never navigates. -DSELECTA -DSONGS -DWORK
#   Violated invariant: desk_library_invalidate called desk_page_load, which is page ENTRY (view source, Library scroll, lib_set_mode, the
#   Search query resend). Setup: the Library page with the view source on the queue; an input script then sends `exec LIBRARY_SCAN <folder>`
#   (a DB write that moves library_serial). The stats show library_reloads >= 1 (the follow ran: the sentinel) and view_source still queue.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()
file(WRITE "${WORK}/in.txt" "wait 1\nexec VIEW_SOURCE queue\nwait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\n")
dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view library --canvas 900x600 --clock fixed --stats
  --input "${WORK}/in.txt")
if(NOT "${o}" MATCHES "library_serial=([0-9]+) library_reloads=([0-9]+) ")
  message(FATAL_ERROR "no library_serial line in:\n${o}")
endif()
if(CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "library_reloads=${CMAKE_MATCH_2}: the follow did not run after the scan (the test proves nothing):\n${o}")
endif()
if(NOT "${o}" MATCHES "view_source=([a-z]+)")
  message(FATAL_ERROR "no view_source line in:\n${o}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL "queue")
  message(FATAL_ERROR "the library change navigated: view_source=${CMAKE_MATCH_1}, want queue:\n${o}")
endif()
