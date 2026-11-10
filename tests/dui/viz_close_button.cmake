# dui-viz-close (I1248): the visual layout has a close button in the upper right corner. -DSELECTA -DSONGS -DWORK -DSDL_BUILT
# Johan 2026-11-10 (Mac): "how do i exit fullscreen visuals? there needs to be a close button in the upper right visible when
# hovering the visualizer". Live runs in the visual layout (entered with the PREF_SET the Visual Layout action C-4 runs), silent:
#   no pointer move    -> the button is not drawn (viz_close paints 0, shown 0)
#   move on the canvas -> the button is drawn (paints > 0, shown 1); a move onto it lights it (hot 1)
#   move, then idle past the hide delay -> hidden again (shown 0)
#   click on it        -> the layout leaves the visual layout (the layout_full action line, what C-1 runs): "layout 0".
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
function(live_run outvar timed)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(MAKE_DIRECTORY "${WORK}/cfg")
  file(WRITE "${WORK}/cfg/prefs.conf" "ui.desktop.viz = viz_spectrum\nui.desktop.mode = full\n")
  set(ENV{SELECTA_SDL_TEST_TIMED} "${timed}")
  execute_process(
    COMMAND "${SELECTA}" --output null --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend sdl --view home
            --canvas 1280x800 --stats --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec "PREF_SET ui.desktop.mode visual"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  endif()
  set(${outvar} "${out}" PARENT_SCOPE)
endfunction()
# Reads the viz_close and viz stats lines into cs_shown cs_hot cs_paints cs_x cs_y cs_w cs_h cs_layout.
macro(close_state out)
  if(NOT "${out}" MATCHES "viz_close shown ([0-9]+) hot ([0-9]+) paints ([0-9]+) rect (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)")
    message(FATAL_ERROR "no viz_close line in:\n${out}")
  endif()
  set(cs_shown "${CMAKE_MATCH_1}")
  set(cs_hot "${CMAKE_MATCH_2}")
  set(cs_paints "${CMAKE_MATCH_3}")
  set(cs_x "${CMAKE_MATCH_4}")
  set(cs_y "${CMAKE_MATCH_5}")
  set(cs_w "${CMAKE_MATCH_6}")
  set(cs_h "${CMAKE_MATCH_7}")
  if(NOT "${out}" MATCHES "viz frames [^\n]* layout ([0-9]+) ")
    message(FATAL_ERROR "no viz line in:\n${out}")
  endif()
  set(cs_layout "${CMAKE_MATCH_1}")
endmacro()

# No move: not drawn.
live_run(rest "q@1200")
close_state("${rest}")
if(NOT cs_layout EQUAL 3 OR NOT cs_paints EQUAL 0 OR NOT cs_shown EQUAL 0)
  message(FATAL_ERROR "rest run: layout ${cs_layout} (want 3), shown ${cs_shown}, paints ${cs_paints} (want 0)")
endif()
if(cs_w LESS_EQUAL 0 OR cs_h LESS_EQUAL 0)
  message(FATAL_ERROR "the close button has no rectangle: ${cs_x} ${cs_y} ${cs_w} ${cs_h}")
endif()
if(NOT cs_x GREATER 640 OR NOT cs_y LESS 100)
  message(FATAL_ERROR "the button is not in the upper right corner: ${cs_x},${cs_y}")
endif()
math(EXPR cx "${cs_x} + ${cs_w} / 2")
math(EXPR cy "${cs_y} + ${cs_h} / 2")

# A move over the visualizer, away from the button: drawn, not lit.
live_run(moved "m400,400@600 q@1200")
close_state("${moved}")
if(NOT cs_shown EQUAL 1 OR NOT cs_paints GREATER 0 OR NOT cs_hot EQUAL 0)
  message(FATAL_ERROR "after a move: shown ${cs_shown} hot ${cs_hot} paints ${cs_paints} (want 1, 0, >0)")
endif()

# A move onto the button: lit.
live_run(onbtn "m${cx},${cy}@600 q@1200")
close_state("${onbtn}")
if(NOT cs_shown EQUAL 1 OR NOT cs_hot EQUAL 1)
  message(FATAL_ERROR "on the button: shown ${cs_shown} hot ${cs_hot} (want 1, 1)")
endif()

# An idle pointer hides it again.
live_run(idle "m400,400@300 q@3800")
close_state("${idle}")
if(NOT cs_shown EQUAL 0 OR NOT cs_paints GREATER 0)
  message(FATAL_ERROR "after the idle delay: shown ${cs_shown} (want 0), paints ${cs_paints} (want >0: it was drawn once)")
endif()

# A click on the button runs layout_full: the layout is no longer the visual layout.
live_run(clicked "m${cx},${cy}@600 d1,${cx},${cy}@700 u1,${cx},${cy}@750 q@1500")
close_state("${clicked}")
if(NOT cs_layout EQUAL 0)
  message(FATAL_ERROR "a click on the close button: layout ${cs_layout}, want 0 (full)")
endif()
message(STATUS "close button: hidden at rest, drawn on a move (${cs_paints} paints), lit on hover, hidden when idle, a click returns to layout 0")
