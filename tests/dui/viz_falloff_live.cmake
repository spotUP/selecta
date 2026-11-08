# dui-viz-falloff-live (I292b): the spectrum falloff runs in the real desktop loop. -DSELECTA -DWORK -DSONGS -DSDL_BUILT
# One reachability test: a live run (SELECTA_SDL_TEST_LIVE, the real loop, silent: --output null) with a song playing and
# the spectrum visualizer; the sentinel viz_falloff_runs (steps the engine's falloff ran, read from the shown viz frame)
# must be > 0. The analysis and the fall rate are tested where they live (dui-viz-spectrum-unit).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
dui_require_sdl()
set(fx "${HERE}/../m3/lyrics")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "ui.desktop.viz = viz_spectrum\nui.desktop.mode = full\n")
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SDL_TEST_TIMED} "q@2500")
dui_run(o --output null --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --view home --canvas 1280x800
  --exec "QUEUE_ADD ${fx}/ambient.mod" --exec PLAY --backend sdl --stats)
dui_stat(runs "${o}" desktop viz_falloff_runs)
if(runs LESS 1)
  message(FATAL_ERROR "the spectrum falloff never ran in the live loop (viz_falloff_runs=${runs}):\n${o}")
endif()
message(STATUS "viz_falloff_runs=${runs}")
