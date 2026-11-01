# dui-sources-repaint (I826): a SOURCES answer that lands after the page has painted repaints the page with no input.
# -DSELECTA -DWORK -DSDL_BUILT. Design: I678 source pages (Browse Sources tiles, source page status block), I265 SOURCES rows.
# Entry point: the live SDL loop (dummy video driver, timed events), where the launch SOURCES reply arrives after the first frame.
# Pages: Browse, and a source page opened from the Sources chip of the sidebar; after the last event only `q` arrives, no mouse, no wheel.
# Proof: "srcpaint gen= painted=" is the count of SOURCES rows that changed what the pages draw, and its value at the last whole
# canvas repaint. painted == gen with gen > 0 means the answer reached the screen. Before the fix (repaint only on the Library page)
# painted stayed behind gen on Browse until the next input.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SOCKET} "/tmp/selecta-dui-sources-repaint.sock")
foreach(view browse library)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(REMOVE "${WORK}/${view}.db")
  set(ENV{SELECTA_SDL_TEST_TIMED} "q@1500")
  dui_run(o --db "${WORK}/${view}.db" --ui desktop --backend sdl --view ${view} --canvas 1280x800 --clock fixed --stats)
  if(NOT o MATCHES "srcpaint gen=([0-9]+) painted=([0-9]+)")
    message(FATAL_ERROR "${view}: no srcpaint line in:\n${o}")
  endif()
  set(gen ${CMAKE_MATCH_1})
  set(painted ${CMAKE_MATCH_2})
  if(gen LESS 1)
    message(FATAL_ERROR "${view}: the launch SOURCES answer never changed a row (gen=${gen}); the sentinel proves nothing")
  endif()
  if(NOT painted EQUAL gen)
    message(FATAL_ERROR "${view}: the SOURCES answer (gen=${gen}) was not painted (painted=${painted}) with no input after it")
  endif()
  message(STATUS "dui-sources-repaint ${view} ok (gen=${gen} painted=${painted})")
endforeach()
