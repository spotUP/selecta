# dui-direct-calls-async (I906e): the desktop UI's direct song text calls never block the UI thread.
# -DSELECTA -DWORK -DSDL_BUILT
# Design: thoughts/shared/research/2026-11-01-ui-thread-library-waits.md section 2c, row I906e.
# Before: queue_groups_sync asked the library for the album group of every queue row (sel_engine_song_group), the Remove dialog and the
# toast for the title, the Song Information page for the metadata: each a watched wait on the pump thread. Now all three are served from
# the engine song text cache (sel_engine_song_text) and resolved on the rd_txt posted read.
# Driven through the desktop UI's live SDL loop on the Queue page with a queue of three songs. SELECTA_LIB_TEXT_SLOW_MS = 300 slows every
# title, group and metadata read in the library worker. Asserts:
# 1. pump_ms_max < 50 and turn_ms_max < 50: nothing slept the 300 ms of a slowed read;
# 2. lib_ui_watched_waits == 0;
# 3. sentinel: view_reads text_reads>=1, the posted text read ran and landed.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
if(NOT SDL_BUILT)
  return()
endif()
set(ENV{SDL_VIDEODRIVER} dummy)
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-direct-calls-async needs the sqlite3 command line tool to load the fixture library")
endif()
set(fx "${HERE}/../m3/lyrics")
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SOCKET} "/tmp/selecta-dui-direct-calls.sock")
set(ENV{SELECTA_LIB_TEXT_SLOW_MS} 300)
# SELECTA_LIB_ART_SLOW_MS is not set: the desktop UI plugin still asks the library for art ids through its own provider (art.find, a watched wait
# outside the engine view path), so a slowed art op would fail this test for a reason that is not I906d. The art id path is proved by its sentinel.
set(ENV{SELECTA_LIB_UI_GUARD} "log")
file(REMOVE_RECURSE "${WORK}/cfg")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_SDL_TEST_TIMED} "q@5000")
# No --output null: that loop waits for posted reads and would inflate pump_ms_max; the live SDL loop is the one that must not wait.
# No OPEN: opening a song runs the play-start reads (loudness, sticker, genre), which are row I906g and watched waits of their own. The
# Remove dialog and the Song Information page read the same cache and are driven by dui-library-remove and dui-songinfo.
set(args --config-dir "${WORK}/cfg" --db "${db}" --ui desktop --backend sdl --view queue --canvas 1280x800 --clock fixed --stats
  --exec "QUEUE_ADD \"fixture/1.mod\"" --exec "QUEUE_ADD \"fixture/2.mod\"" --exec "QUEUE_ADD \"fixture/3.xm\"")
execute_process(COMMAND "${SELECTA}" ${args} OUTPUT_VARIABLE o ERROR_VARIABLE o_err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${o_err}")
endif()
if(NOT "${o}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+) lib_ui_guard_hits=([0-9]+) lib_ui_allowed_waits=([0-9]+) lib_ui_watched_waits=([0-9]+)")
  message(FATAL_ERROR "no ui_stats_pump line in:\n${o}")
endif()
set(pump ${CMAKE_MATCH_1})
if(pump GREATER 49)
  message(FATAL_ERROR "pump_ms_max is ${pump}, want < 50:\n${o}")
endif()
if(CMAKE_MATCH_4 GREATER 0)
  message(FATAL_ERROR "lib_ui_watched_waits is ${CMAKE_MATCH_4}, want 0:\n${o}")
endif()
if(NOT "${o}" MATCHES "turn_ms_max=([0-9]+)")
  message(FATAL_ERROR "no turn_ms_max in:\n${o}")
endif()
if(CMAKE_MATCH_1 GREATER 49)
  message(FATAL_ERROR "turn_ms_max is ${CMAKE_MATCH_1}, want < 50:\n${o}")
endif()
set(turn ${CMAKE_MATCH_1})
if(NOT "${o}" MATCHES "view_reads panel_reads=([0-9]+) art_reads=([0-9]+) total_reads=([0-9]+) text_reads=([0-9]+)")
  message(FATAL_ERROR "no view_reads line in:\n${o}")
endif()
if(CMAKE_MATCH_4 LESS 1)
  message(FATAL_ERROR "the posted song text read never ran (text_reads=${CMAKE_MATCH_4}):\n${o}")
endif()
message(STATUS "direct_calls_async_ok pump_ms_max=${pump} turn_ms_max=${turn} text_reads=${CMAKE_MATCH_4}")
