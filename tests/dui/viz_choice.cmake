# dui-viz-choice (I1230a, I1052w): the three ways a user picks the visualiser all reach the drawing plugin, through the real
# `selecta --ui desktop` entry point. -DSELECTA -DWORK. Each run starts from ui.desktop.viz=viz_levels in prefs.conf:
# 1. The stored pref alone: the desktop opens drawing viz_levels (the desktop read it only from PREFS_CHANGED, so a
#    stored pick was ignored at open and it drew viz_scope).
# 2. `viz 3`, a click on switch button 3: viz_sidregs. From Levels, button 3 is not next to the selected plugin, so in
#    the "< name >" control (the strip is too narrow for every tab) the click steps the chevrons until it is drawn
#    (it clicked the empty rect at 0,0 and nothing changed).
# 3. ctrl+8, the View/Visualizer/SID PatternScope key: viz_sidregs.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
function(choice_run tag script want)
  file(REMOVE_RECURSE "${WORK}/cfg" "${WORK}/${tag}.db")
  file(MAKE_DIRECTORY "${WORK}/cfg")
  file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\nui.desktop.viz=viz_levels\n")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${WORK}/${tag}.db" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
    --exec "OPEN ${HERE}/../m3/lyrics/ambient.mod" --ticks 12 --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "viz frames [0-9]+ song_ms [0-9]+ plugin ([a-z_]+) plugins ([0-9]+)")
    message(FATAL_ERROR "${tag}: no viz sentinel in:\n${o}")
  endif()
  if(NOT CMAKE_MATCH_1 STREQUAL "${want}")
    message(FATAL_ERROR "${tag}: drawing plugin ${CMAKE_MATCH_1} of ${CMAKE_MATCH_2}, want ${want}")
  endif()
  message(STATUS "${tag}: ${CMAKE_MATCH_1} of ${CMAKE_MATCH_2}")
endfunction()
choice_run(pref "wait 100\n" viz_levels)
choice_run(click "wait 100\nviz 3\nwait 100\n" viz_sidregs)
choice_run(key "wait 100\nkey ctrl+8\nwait 100\n" viz_sidregs)
