# dui-uim-frame (DUI-P1-4): through the real shell, the desktop UI's model reports which regions a NEXT changed.
#   -DSELECTA=<exe> -DSONGS=<dir with the generated .mod files> -DWORK=<dir>
# Three songs, PLAY, NEXT; then one more NEXT adds exactly the song and queue widgets (dirty bits 1 and 2 only,
# widget draws +1 or +2): play state, clock and volume widgets stay untouched.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
set(base --ui desktop --backend mem --stats
  --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec "QUEUE_ADD ${SONGS}/selecta-short.mod"
  --exec "QUEUE_ADD ${SONGS}/selecta-loop.mod" --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec PLAY --exec NEXT)
dui_run(one ${base})
dui_run(two ${base} --exec NEXT)
foreach(run one two)
  foreach(key frames widget_draws uim_updates uim_dirty)
    dui_stat(${run}_${key} "${${run}}" desktop ${key})
  endforeach()
  if(${run}_frames EQUAL 0 OR ${run}_uim_updates EQUAL 0)
    message(FATAL_ERROR "${run}: the model never ran (frames=${${run}_frames} updates=${${run}_uim_updates})")
  endif()
endforeach()
if(one_widget_draws LESS 7)
  message(FATAL_ERROR "the first frame draws 6 widgets and the later changes more: ${one_widget_draws}")
endif()
# The engine opens the song a turn after the queue moves, so the last frame carries one of the two bits; no other
# bit. Four songs keep the previous and next buttons enabled in both runs.
math(EXPR other "${two_uim_dirty} & ~3")
if(NOT other EQUAL 0 OR two_uim_dirty EQUAL 0)
  message(FATAL_ERROR "NEXT must dirty only song (1) and queue (2), the last frame carried ${two_uim_dirty}")
endif()
math(EXPR delta "${two_widget_draws} - ${one_widget_draws}")
if(delta LESS 1 OR delta GREATER 2)
  message(FATAL_ERROR "the second NEXT must draw the song and queue widgets only (1 or 2), drew ${delta}")
endif()
