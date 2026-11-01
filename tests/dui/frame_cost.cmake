# dui-frame-cost (I845): a playing song must not regenerate its covers on every frame, and a frame with nothing changed redraws
# nothing. Runs the real live desktop loop on the dummy SDL driver for 3 s with a song playing (--stats, null output).
# `cover_cache generated= hits=` counts the platform covers drawn from scratch (misses) and blitted from the LRU (hits); the
# `platform_covers ... draws=` sentinel counts every fallback cover draw, so generated must stay below the draws (hits grow).
# Idle frames: `ui_stats frames=` counts state-change frames and `turns present=` the presented turns; a 3 s idle-but-playing
# run allows the clock seconds and a few progress bar steps only.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SDL_TEST_TIMED} "q@3000")
set(ENV{SELECTA_SOCKET} "${WORK}/ctl.sock")

dui_run(o --ui desktop --backend sdl --view home --canvas 1280x800 --stats --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec PLAY)
if(NOT "${o}" MATCHES "cover_cache generated=([0-9]+) hits=([0-9]+)")
  message(FATAL_ERROR "no cover_cache line in:\n${o}")
endif()
set(gen ${CMAKE_MATCH_1})
set(hits ${CMAKE_MATCH_2})
if(NOT "${o}" MATCHES "platform_covers [^\n]* draws=([0-9]+)")
  message(FATAL_ERROR "no platform_covers line in:\n${o}")
endif()
set(draws ${CMAKE_MATCH_1})
message(STATUS "cover_cache generated=${gen} hits=${hits} draws=${draws}")
if(draws LESS 2)
  message(FATAL_ERROR "the playing home view drew ${draws} fallback covers: the cover sentinel did not run")
endif()
if(gen LESS 1 OR hits LESS 1 OR NOT gen LESS draws)
  message(FATAL_ERROR "covers generated=${gen} hits=${hits} for ${draws} draws: every draw regenerated its cover")
endif()
if(NOT "${o}" MATCHES "ui_stats id=desktop frames=([0-9]+) ")
  message(FATAL_ERROR "no frames in:\n${o}")
endif()
if(CMAKE_MATCH_1 GREATER 12)
  message(FATAL_ERROR "playing and idle: ${CMAKE_MATCH_1} frames redrawn in 3 s (expected at most 12)")
endif()
