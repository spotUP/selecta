# dui-scan-ui (I315, I316, I297): the Add Folder scan shows its progress, ends on a fresh Library page and toasts clear of the controls.
#   -DSELECTA -DSONGS -DWORK
# The scan runs on the engine's command thread; the dump reads the live progress the UI drew last (scan_ui line).
# 1. Add Folder on the Library page over a folder of three songs: the UI saw the progress finish at done=3 total=3 on the
#    last file (file=c.mod), the engine scanned 3 files, a toast "Added 3 songs" is up and the toast rectangle does
#    not intersect the Add Folder button or the progress rectangle (I297).
# 2. A folder dropped on the Home page stays on Home (page 0) and the scan finishes with progress 3 of 3.
# 3. I336: the scan runs on the library job thread (see below).
# 4. I506: three folders dropped back to back are all scanned.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()

function(scan_run outvar view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(REMOVE "${WORK}/library.db")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --start-dir "${WORK}/songs" --db "${WORK}/library.db" --ui desktop --backend mem --view ${view} --canvas 900x600
    --clock fixed --stats --dialog-answer "${WORK}/songs" --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(rects_apart a b what)
  # a and b are "x y w h" lists
  list(GET a 0 ax) 
  list(GET a 1 ay)
  list(GET a 2 aw)
  list(GET a 3 ah)
  list(GET b 0 bx)
  list(GET b 1 by)
  list(GET b 2 bw)
  list(GET b 3 bh)
  math(EXPR ar "${ax} + ${aw}")
  math(EXPR ab "${ay} + ${ah}")
  math(EXPR br "${bx} + ${bw}")
  math(EXPR bb "${by} + ${bh}")
  if(ax LESS br AND bx LESS ar AND ay LESS bb AND by LESS ab)
    message(FATAL_ERROR "${what}: rectangle (${a}) intersects (${b})")
  endif()
endfunction()

scan_run(idle library "wait 1\n")
if(NOT "${idle}" MATCHES "scan_ui page=([0-9]+) done=0 total=0 file= draws=0 pending=0 toast_rect")
  message(FATAL_ERROR "no idle scan_ui line:\n${idle}")
endif()
if(NOT "${idle}" MATCHES "addfolder ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no addfolder line:\n${idle}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

# 1. Add Folder on the Library page.
scan_run(added library "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 5\n")
if(NOT "${added}" MATCHES "scan_ui page=2 done=3 total=3 file=c.mod draws=[0-9]+ pending=0 ")
  message(FATAL_ERROR "scan progress not seen as 3 of 3 on the last file:\n${added}")
endif()
# CMake keeps nine match groups, so each rectangle gets its own match
if(NOT "${added}" MATCHES "toast_rect (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no toast_rect:\n${added}")
endif()
set(toast "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4}")
if(NOT "${added}" MATCHES " add_rect (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no add_rect:\n${added}")
endif()
set(addr "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4}")
if(NOT "${added}" MATCHES " scan_rect (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no scan_rect:\n${added}")
endif()
set(scanr "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4}")
if(NOT "${added}" MATCHES "scan probed [1-9][0-9]* files 3 scans 1")
  message(FATAL_ERROR "the folder was not scanned once:\n${added}")
endif()
if(NOT "${added}" MATCHES "toasts=1 toast_on=1")
  message(FATAL_ERROR "no 'Added 3 songs' toast is up:\n${added}")
endif()
rects_apart("${toast}" "${addr}" "toast vs Add Folder")
rects_apart("${toast}" "${scanr}" "toast vs scan progress")

# 2. A drop on the Home page keeps the page; the scan still reports 3 of 3.
scan_run(home home "wait 1\ndrop ${WORK}/songs\nwait 5\n")
if(NOT "${home}" MATCHES "scan_ui page=0 done=3 total=3 file=c.mod ")
  message(FATAL_ERROR "a folder dropped on Home changed page or lost progress:\n${home}")
endif()
if(NOT "${home}" MATCHES "scan probed [1-9][0-9]* files 3 scans 1")
  message(FATAL_ERROR "Home drop did not scan:\n${home}")
endif()

# 3. I336: the scan runs on the library job thread, so the shell keeps turning while it runs. With the scan thread slowed to
#    20 ms a file over 12 files the UI sees progress snapshots with done < total (scan_mid) and still ends at 12 of 12; a
#    synchronous scan inside the engine pump never shows a snapshot in the middle.
file(MAKE_DIRECTORY "${WORK}/many")
foreach(n a b c d e f g h i j k l)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/many/${n}.mod")
endforeach()
file(REMOVE_RECURSE "${WORK}/cfg")
file(REMOVE "${WORK}/library.db")
file(WRITE "${WORK}/in.txt" "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 5\n")
set(ENV{SELECTA_SCAN_DELAY_MS} 20)
dui_run(slow --start-dir "${WORK}/many" --db "${WORK}/library.db" --ui desktop --backend mem --view library --canvas 900x600
  --clock fixed --stats --dialog-answer "${WORK}/many" --input "${WORK}/in.txt")
unset(ENV{SELECTA_SCAN_DELAY_MS})
if(NOT "${slow}" MATCHES "scan_mid ([0-9]+)\n")
  message(FATAL_ERROR "no scan_mid line:\n${slow}")
endif()
if(CMAKE_MATCH_1 LESS 3)
  message(FATAL_ERROR "the UI saw only ${CMAKE_MATCH_1} mid-scan snapshots; the scan blocked the shell loop:\n${slow}")
endif()
if(NOT "${slow}" MATCHES "scan_ui page=2 done=12 total=12 ")
  message(FATAL_ERROR "scan did not end at 12 of 12:\n${slow}")
endif()

# 4. I506: one drop of three folders sends three LIBRARY_SCANs back to back while the first still scans (100 ms a file).
#    The engine queues the second and third instead of answering scan_busy, so the UI sees three results and the library
#    holds all 2 + 3 + 4 songs.
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-scan-ui needs the sqlite3 command line tool to count the scanned songs")
endif()
set(drops "wait 1\ndrop")
set(n 2)
foreach(dir x y z)
  file(MAKE_DIRECTORY "${WORK}/multi/${dir}")
  foreach(i RANGE 1 ${n})
    file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/multi/${dir}/${dir}${i}.mod")
  endforeach()
  string(APPEND drops " ${WORK}/multi/${dir}")
  math(EXPR n "${n} + 1")
endforeach()
string(APPEND drops "\nwait 5\n")
set(ENV{SELECTA_SCAN_DELAY_MS} 100)
scan_run(multi home "${drops}")
unset(ENV{SELECTA_SCAN_DELAY_MS})
if(NOT "${multi}" MATCHES "scan probed [0-9]+ files [0-9]+ scans 3\n")
  message(FATAL_ERROR "three dropped folders did not give three scan results:\n${multi}")
endif()
if(NOT "${multi}" MATCHES "scan_ui page=0 [^\n]* pending=0 ")
  message(FATAL_ERROR "a scan is still pending after the three drops:\n${multi}")
endif()
execute_process(COMMAND "${SQLITE3}" "${WORK}/library.db" "SELECT COUNT(*) FROM songs WHERE path LIKE '%/multi/%'"
  OUTPUT_VARIABLE songs OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT songs STREQUAL "9")
  message(FATAL_ERROR "the library holds ${songs} of the 9 songs of the three dropped folders (rc ${rc})")
endif()
message(STATUS "scan_ui_multi_drop_ok")
