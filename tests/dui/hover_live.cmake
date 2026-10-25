# dui-hover-live (I341): hover shows in the live window. -DSELECTA -DSONGS -DWORK -DSDL_BUILT
# The live loop (plugins/ui/desktop/desktop_ui.c, the `if (d->live)` turn, shared by the sdl and rtg backends) waits in
# poll for up to a second and then routes the event. A hover tween started on the clock of the turn before was over on the
# next turn, so the hovered control kept the fill it had at weight 0 and its end state was never drawn. The scripted
# `--input` runs keep the clock current and cannot see it, so this runs the real live loop: SELECTA_SDL_TEST_LIVE=1 makes
# the dummy driver a live display, SELECTA_SDL_TEST_TIMED pushes one mouse motion over the New Playlist button 700 ms in
# (while the loop waits in poll) and a quit at 2200 ms. The stats line `hover_box x y w h fill=F token=T paints=N` must
# show that button hovered with its last painted fill equal to the surface-hover token.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
if(NOT SDL_BUILT)
  return()
endif()
set(ENV{SDL_VIDEODRIVER} dummy)
dui_run(o0 --ui desktop --backend mem --view playlists --canvas 1280x800 --clock fixed --stats)
string(REGEX MATCH "label (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) ink [-0-9]+ [-0-9]+ text New Playlist\n" _m "${o0}")
if(_m STREQUAL "")
  message(FATAL_ERROR "no 'New Playlist' label in the stats:\n${o0}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(box "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4}")
file(REMOVE_RECURSE "${WORK}/cfg")
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SDL_TEST_TIMED} "m${cx},${cy}@700 q@2200")
dui_run(o1 --ui desktop --backend sdl --view playlists --canvas 1280x800 --stats)
string(REGEX MATCH "hover_box (-?[0-9]+ -?[0-9]+ -?[0-9]+ -?[0-9]+) fill=([0-9a-f]+) token=([0-9a-f]+) paints=([0-9]+)" _m "${o1}")
if(_m STREQUAL "")
  message(FATAL_ERROR "no hover_box line in:\n${o1}")
endif()
if(NOT "${CMAKE_MATCH_4}" GREATER 0)
  message(FATAL_ERROR "the live loop never painted the hovered control (paints=0)")
endif()
if(NOT "${CMAKE_MATCH_1}" STREQUAL "${box}")
  message(FATAL_ERROR "the hovered control is '${CMAKE_MATCH_1}', expected the New Playlist button '${box}'")
endif()
if(NOT "${CMAKE_MATCH_2}" STREQUAL "${CMAKE_MATCH_3}")
  message(FATAL_ERROR "the hovered fill ${CMAKE_MATCH_2} is not the surface-hover token ${CMAKE_MATCH_3}: the end state of the fade was never drawn")
endif()
