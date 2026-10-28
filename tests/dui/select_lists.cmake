# dui-select-lists (I632d, plan 2026-10-28-multi-select; label select-lists, not a plan id): multi-select on the desktop lists.
#   -DSELECTA -DSONGS -DWORK
#   Violated invariant: a list row is a selectable row. Every song row a list registers carries its list kind, id, version and
#   index, so a plain click selects one row, a Control (Command on macOS) click toggles, a Shift click extends from the anchor,
#   Control+A selects the whole list, Shift+Down extends by one and Escape clears. The `select` line of --stats gives the count,
#   the run count and the ranges the single model (src/uimodel/select.c) holds. A sentinel (clicks=, keys=) proves the desktop
#   wiring ran. Lists covered: the Queue page, the Library view, Search.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c d)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()
set(db "${WORK}/library.db")

function(sl_run outvar view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --start-dir "${WORK}/songs" --db "${db}" --ui desktop --backend mem --view ${view} --canvas 1280x800 --clock fixed --stats
          --exec "QUEUE_ADD ${WORK}/songs/a.mod" --exec "QUEUE_ADD ${WORK}/songs/b.mod" --exec "QUEUE_ADD ${WORK}/songs/c.mod"
          --exec "QUEUE_ADD ${WORK}/songs/d.mod" --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# sel_expect(<what> <view> <input> <count> <runs> <ranges>)
function(sel_expect what view input count runs ranges)
  sl_run(out ${view} "${input}")
  if(NOT "${out}" MATCHES "select count=([0-9]+) runs=([0-9]+) list=([0-9]+),([0-9]+),([0-9]+) ranges=([^ ]+) clicks=([0-9]+) keys=([0-9]+)")
    message(FATAL_ERROR "${what}: no select line in:\n${out}")
  endif()
  if(NOT CMAKE_MATCH_1 EQUAL ${count} OR NOT CMAKE_MATCH_2 EQUAL ${runs} OR NOT "${CMAKE_MATCH_6}" STREQUAL "${ranges}")
    message(FATAL_ERROR "${what}: selection ${CMAKE_MATCH_1} rows in ${CMAKE_MATCH_2} runs (${CMAKE_MATCH_6}), expected ${count} in ${runs} (${ranges}); clicks=${CMAKE_MATCH_7} keys=${CMAKE_MATCH_8}")
  endif()
endfunction()

# The Queue page: the row rectangles of the dump.
sl_run(q0 queue "wait 2\n")
foreach(k 0 1 2)
  if(NOT "${q0}" MATCHES "row ${k} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
    message(FATAL_ERROR "queue: no row ${k} rectangle in:\n${q0}")
  endif()
  math(EXPR rx${k} "${CMAKE_MATCH_1} + 20")
  math(EXPR ry${k} "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
endforeach()
set(pre "wait 2\n")
sel_expect("queue plain click" queue "${pre}move ${rx1} ${ry1}\nclick ${rx1} ${ry1}\nwait 1\n" 1 1 "1")
sel_expect("queue ctrl click toggles" queue "${pre}click ${rx0} ${ry0}\nclick ${rx2} ${ry2} 1 ctrl\nwait 1\n" 2 2 "0,2")
sel_expect("queue shift click extends" queue "${pre}click ${rx0} ${ry0}\nclick ${rx2} ${ry2} 1 shift\nwait 1\n" 3 1 "0-2")
sel_expect("queue ctrl click removes" queue "${pre}click ${rx0} ${ry0}\nclick ${rx2} ${ry2} 1 shift\nclick ${rx1} ${ry1} 1 ctrl\nwait 1\n" 2 2 "0,2")
sel_expect("queue select all" queue "${pre}key ctrl+a\nwait 1\n" 4 1 "0-3")
sel_expect("queue escape clears" queue "${pre}key ctrl+a\nkey escape\nwait 1\n" 0 0 "-")
sel_expect("queue shift down extends" queue "${pre}click ${rx0} ${ry0}\nkey shift+down\nkey shift+down\nwait 1\n" 3 1 "0-2")
