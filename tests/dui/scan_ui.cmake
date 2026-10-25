# dui-scan-ui (I315, I316, I297): the Add Folder scan shows its progress, ends on a fresh Library page and toasts clear of the controls.
#   -DSELECTA -DSONGS -DWORK
# The scan runs on the engine's command thread; the dump reads the live progress the UI drew last (scan_ui line).
# 1. Add Folder on the Library page over a folder of three songs: the UI saw the progress finish at done=3 total=3 on the
#    last file (file=c.mod), the engine scanned 3 files, a toast "Added 3 songs" is up and the toast rectangle does
#    not intersect the Add Folder button or the progress rectangle (I297).
# 2. A folder dropped on the Home page stays on Home (page 0) and the scan finishes with progress 3 of 3.
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
