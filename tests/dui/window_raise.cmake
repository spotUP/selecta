# window-raise (I501): the SDL desktop window raises and takes focus when it opens. -DSELECTA -DSONGS -DWORK -DSDL_BUILT
# The stats line `window raise_count=N` counts the SDL_RaiseWindow calls desk_sdl_open made; without the call it is 0.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
if(NOT SDL_BUILT)
  return()
endif()
set(ENV{SDL_VIDEODRIVER} dummy)
dui_run(o --ui desktop --backend sdl --view queue --canvas 1280x800 --clock fixed --stats
  --exec "QUEUE_ADD ${SONGS}/selecta-test.mod")
if(NOT "${o}" MATCHES "window raise_count=([0-9]+)")
  message(FATAL_ERROR "no raise_count in:\n${o}")
endif()
if(NOT "${CMAKE_MATCH_1}" STREQUAL "1")
  message(FATAL_ERROR "the window must be raised exactly once at open, raise_count=${CMAKE_MATCH_1}")
endif()
