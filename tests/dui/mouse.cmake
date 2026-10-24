# dui-mouse (DUI-P3-2): the mouse in the desktop songs view, through the real shell and the scripted --input feeder.
#   -DSELECTA -DSONGS -DWORK
# Three copies of the test module are queued as banana, apple and cherry. At 1280x800, scale 1: the rows of the songs
# list start at y=110 (32 high), the title column header is at y=84, the sidebar Queue drop target is at (50, 50).
# 1. A double-click on row 0 starts that song: the engine's `song` path equals the row's path.
# 2. Dragging row 1 onto the Queue target adds one entry (3 queued, 4 after).
# 3. A header click sorts ascending (apple first), a second one descending (cherry first), as the plan states.
# 4. Moving over a row counts a hover and a secondary click opens the context menu.
# Every stat is a sentinel that the pointer code ran: the counters rise only through sel_uim_hit and desk_pointer.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n banana apple cherry)
  configure_file("${SONGS}/selecta-test.mod" "${WORK}/${n}.mod" COPYONLY)
endforeach()

function(mouse_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view songs --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${WORK}/banana.mod" --exec "QUEUE_ADD ${WORK}/apple.mod" --exec "QUEUE_ADD ${WORK}/cherry.mod"
    --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(expect out key want)
  if(key STREQUAL "playlist_entries") # a top-level line, not in the ui_stats line
    if(NOT "${out}" MATCHES "playlist_entries=([0-9]+)")
      message(FATAL_ERROR "no playlist_entries in:\n${out}")
    endif()
    set(got "${CMAKE_MATCH_1}")
  else()
    dui_stat(got "${out}" desktop ${key})
  endif()
  if(NOT got EQUAL want)
    message(FATAL_ERROR "${key}=${got}, expected ${want} in:\n${out}")
  endif()
endfunction()

# Baseline: no pointer input.
mouse_run(base "move 5 5\n")
expect("${base}" playlist_entries 3)

# 1. Double-click on row 0 plays it.
mouse_run(dbl "dclick 600 110\n")
if(NOT dbl MATCHES "song=([^\n]*)\n")
  message(FATAL_ERROR "no song line in:\n${dbl}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL "${WORK}/banana.mod")
  message(FATAL_ERROR "double-click played '${CMAKE_MATCH_1}', expected ${WORK}/banana.mod")
endif()
expect("${dbl}" double_clicks 1)
expect("${dbl}" row_plays 1)

# 2. Drag a row to the queue.
mouse_run(drag "drag 600 142 50 50\n")
expect("${drag}" drags 1)
expect("${drag}" playlist_entries 4)

# 3. Header click: ascending, then descending.
mouse_run(asc "click 600 84\n")
expect("${asc}" sorts 1)
expect("${asc}" sort_dir 1)
expect("${asc}" top_row 2)
mouse_run(desc "click 600 84\nclick 600 84\n")
expect("${desc}" sorts 2)
expect("${desc}" sort_dir 2)
expect("${desc}" top_row 3)

# 4. Hover and context menu.
mouse_run(menu "move 600 110\nrclick 600 142\n")
expect("${menu}" hovers 1)
expect("${menu}" menus 1)
