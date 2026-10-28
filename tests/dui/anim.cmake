# dui-anim (DUI-P3-5): animation timeline, reduce-motion pref and idle blocking, through the real shell.
#   -DSELECTA -DSONGS -DWORK -DGOLDEN
# 1. Idle: no input and no song, a 10 s run on the fixed clock (`wait 10000`) draws no frame beyond the start (first frame + the Home entry frame, I594).
# 2. A view change (`tile 0` on the home grid) with ui.desktop.reduce_motion=1 draws 1 frame more than the run without
#    it and starts no tween; without the pref it draws more than 1 frame more and starts a tween (sentinel).
# The pref persists in the config dir, so every run sets it.
# 3. The end state is the same either way: the canvas hash with and without motion is identical.
# 4. Unit: the timeline and 64-entry easing tables are integer-only (checked by dui-draw's no-float scan of src/gui).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

file(WRITE "${WORK}/idle.txt" "wait 10000\n")
dui_run(idle --ui desktop --backend mem --stats --clock fixed --input "${WORK}/idle.txt")
dui_stat(idle_frames "${idle}" desktop frames)
# I594: the start enters Home through the page door, so the first frame is followed by one frame when the Home view
# arrives. Idle time itself must add nothing: a 10 s wait draws as many frames as a 100 ms wait.
file(WRITE "${WORK}/idle_short.txt" "wait 100\n")
dui_run(idle_short --ui desktop --backend mem --stats --clock fixed --input "${WORK}/idle_short.txt")
dui_stat(idle_short_frames "${idle_short}" desktop frames)
if(NOT idle_frames EQUAL idle_short_frames OR idle_frames GREATER 2)
  message(FATAL_ERROR "an idle run with no input and no song must draw the first frame plus the start-page frame only, and idle time must add none (10 s drew ${idle_frames}, 100 ms drew ${idle_short_frames})")
endif()

file(STRINGS "${HERE}/fixture_library.sql" album_lines REGEX "^INSERT INTO albums")
set(execs "")
foreach(line IN LISTS album_lines)
  string(FIND "${line}" "VALUES(" at)
  if(NOT at EQUAL -1)
    string(SUBSTRING "${line}" ${at} -1 rest)
    string(FIND "${rest}" "'" q1)
    math(EXPR from "${q1} + 1")
    string(SUBSTRING "${rest}" ${from} -1 rest)
    string(FIND "${rest}" "'" q2)
    string(SUBSTRING "${rest}" 0 ${q2} title)
    list(APPEND execs --exec "QUEUE_ADD \"fixture/${title}.mod\"")
  endif()
endforeach()
set(common --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats ${execs})

dui_run(base ${common} --exec "PREF_SET ui.desktop.reduce_motion 0")
dui_stat(base_frames "${base}" desktop frames)
file(WRITE "${WORK}/change.txt" "tile 0\nwait 400\n")
dui_run(moving ${common} --exec "PREF_SET ui.desktop.reduce_motion 0" --input "${WORK}/change.txt")
dui_run(still ${common} --exec "PREF_SET ui.desktop.reduce_motion 1" --input "${WORK}/change.txt")
foreach(run moving still)
  dui_stat(${run}_frames "${${run}}" desktop frames)
  dui_stat(${run}_tweens "${${run}}" desktop tweens)
  dui_stat(${run}_hash "${${run}}" desktop canvas_hash)
  dui_stat(${run}_opens "${${run}}" desktop view_opens)
  if(NOT ${run}_opens EQUAL 1)
    message(FATAL_ERROR "${run}: the view change did not happen (view_opens=${${run}_opens})")
  endif()
endforeach()
math(EXPR still_want "${base_frames} + 1")
if(NOT still_frames EQUAL still_want OR NOT still_tweens EQUAL 0)
  message(FATAL_ERROR "reduce_motion=1: a view change must draw 1 frame and start no tween (frames ${still_frames}, want ${still_want}; tweens ${still_tweens})")
endif()
if(moving_frames LESS_EQUAL still_want OR moving_tweens LESS 1)
  message(FATAL_ERROR "without reduce_motion the view change must draw tween frames (frames ${moving_frames} <= ${still_want}, tweens ${moving_tweens})")
endif()
if(NOT moving_hash STREQUAL still_hash)
  message(FATAL_ERROR "the tween end state differs from the reduced-motion frame (${moving_hash} vs ${still_hash})")
endif()

# A hover switches at once (I351, I356, I359): one frame more than the base run, with or without reduce_motion, same end state.
file(WRITE "${WORK}/hover.txt" "hover 0\nwait 300\n")
dui_run(hov ${common} --exec "PREF_SET ui.desktop.reduce_motion 0" --input "${WORK}/hover.txt")
dui_run(hov_still ${common} --exec "PREF_SET ui.desktop.reduce_motion 1" --input "${WORK}/hover.txt")
dui_stat(hov_frames "${hov}" desktop frames)
dui_stat(hov_still_frames "${hov_still}" desktop frames)
dui_stat(hov_hash "${hov}" desktop canvas_hash)
dui_stat(hov_still_hash "${hov_still}" desktop canvas_hash)
if(NOT hov_still_frames EQUAL still_want OR NOT hov_frames EQUAL still_want OR NOT hov_hash STREQUAL hov_still_hash)
  message(FATAL_ERROR "hover: frames ${hov_frames} (still ${hov_still_frames}, want ${still_want} for both: no fade), hashes ${hov_hash} / ${hov_still_hash}")
endif()
