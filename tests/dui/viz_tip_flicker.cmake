# dui-viz-tip-flicker (I292a): a tooltip over the visualizer is drawn again by every visualizer frame. -DSELECTA -DWORK -DSONGS -DSDL_BUILT
# viz_tick repaints only the visualizer area for each new engine frame; before the fix it never drew the tooltip, so each
# frame painted over a tooltip overlapping the strip and only the next full frame restored it (alternating flicker).
# A mem run with a playing song finds the Now Playing visualizer strip (`np_viz` on the viz_switch line); a live run
# (SELECTA_SDL_TEST_LIVE, the real loop, silent: --output null) hovers the left chevron of the switch row right under the
# strip, whose tooltip box reaches up into the strip. The stats viz_tip_up (visualizer frames drawn with a tooltip up) and
# viz_tip_drawn (of those, the ones that drew it) must be > 0 and equal.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
dui_require_sdl()
set(fx "${HERE}/../m3/lyrics")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "ui.desktop.viz = viz_spectrum\nui.desktop.mode = full\n")
set(ENV{SDL_VIDEODRIVER} dummy)
set(play --output null --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --view home --canvas 1280x800
  --exec "QUEUE_ADD ${fx}/ambient.mod" --exec PLAY)
dui_run(m ${play} --backend mem --clock fixed --stats --ticks 12)
if(NOT "${m}" MATCHES "np_viz (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)\n" OR CMAKE_MATCH_3 EQUAL 0)
  message(FATAL_ERROR "no Now Playing visualizer strip in:\n${m}")
endif()
math(EXPR hx "${CMAKE_MATCH_1} + 6")
math(EXPR hy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} + 16")
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SDL_TEST_TIMED} "m${hx},${hy}@600 q@3000")
dui_run(o ${play} --backend sdl --stats)
dui_stat(tips "${o}" desktop tooltips)
dui_stat(up "${o}" desktop viz_tip_up)
dui_stat(drawn "${o}" desktop viz_tip_drawn)
if(tips LESS 1)
  message(FATAL_ERROR "no tooltip appeared over the switch chevron at (${hx},${hy}):\n${o}")
endif()
if(up LESS 1)
  message(FATAL_ERROR "no visualizer frame was drawn while the tooltip was up (viz_tip_up=${up})")
endif()
if(NOT up EQUAL drawn)
  message(FATAL_ERROR "${up} visualizer frames drew with a tooltip up but only ${drawn} drew the tooltip: it flickers")
endif()
