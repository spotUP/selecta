# dui-library-reload (DUI-P3-14, I348): the Library view reloads its rows after a scan it did not start. -DSELECTA -DSONGS -DWORK
#   Violated invariant: page state follows library state, whoever changed it. LIBRARY_RESULT is no signal (searches and browses
#   answer with the same event), so the engine counts library changes in sel_state.library_serial and the window reloads its page
#   when that moves. An empty db, `--view library`, and an input script that once the window is up sends `exec LIBRARY_SCAN <folder
#   of three songs>` (the line a CLI, net or ARexx client sends; the window's own scan_pending stays 0). The headless loop keeps
#   turning while the engine reports a scan active, so the script's last wait sees the finished scan. The stats show
#   `library_serial=<n>` with n >= 1 and `library_reloads=<m>` with m >= 1 (the reload ran), and the page totals hold the 3 songs.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()
file(WRITE "${WORK}/in.txt" "wait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\n")
dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view library --canvas 900x600 --clock fixed --stats
  --input "${WORK}/in.txt")
if(NOT "${o}" MATCHES "library_serial=([0-9]+) library_reloads=([0-9]+)\n")
  message(FATAL_ERROR "no library_serial line in:\n${o}")
endif()
if(CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "library_serial=${CMAKE_MATCH_1}: the engine did not count the finished scan")
endif()
if(CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "library_reloads=${CMAKE_MATCH_2}: the Library view did not reload after the scan")
endif()
if(NOT "${o}" MATCHES "\nstat Songs in library\\|3\n")
  message(FATAL_ERROR "the Library page does not show the 3 scanned songs:\n${o}")
endif()
