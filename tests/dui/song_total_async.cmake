# dui-song-total-async (I936): the catalogue song count never blocks the UI thread.
# -DSELECTA -DWORK -DSDL_BUILT
# Design: thoughts/shared/research/2026-11-01-ui-thread-library-waits.md section 4 (posted read slots, the I910 pattern).
# Before: refresh_snapshot ran sel_library_song_total (SELECT COUNT(*) FROM songs) on the pump thread under sel_library_nowait(1).
# nowait only skips a busy worker; a free worker ran the count while the pump waited. On Johan's 780 MB library.db that one
# statement took 1-2.6 s (pump_ms_max 1068 cold, 2614 warm). Now the count rides the rd_total posted read slot.
# Driven through the desktop UI's live SDL loop. SELECTA_LIB_COUNT_SLOW_MS = 300 slows every catalogue count in the library worker.
# Asserts:
# 1. pump_ms_max < 50: the pump never slept the 300 ms of a slowed count;
# 2. sentinel: view_reads total_reads>=1, the posted count ran and landed.
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
  message(FATAL_ERROR "dui-song-total-async needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_LIB_COUNT_SLOW_MS} 300)
set(ENV{SELECTA_LIB_UI_GUARD} "log")
file(REMOVE_RECURSE "${WORK}/cfg")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_SDL_TEST_TIMED} "q@3000")
# No --output null: that loop waits for posted reads and would inflate pump_ms_max; the live SDL loop is the one that must not wait.
execute_process(COMMAND "${SELECTA}" --config-dir "${WORK}/cfg" --db "${db}" --ui desktop --backend sdl --view home --canvas 1280x800
  --clock fixed --stats OUTPUT_VARIABLE o ERROR_VARIABLE o_err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${o_err}")
endif()
if(NOT "${o}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+) lib_ui_guard_hits=([0-9]+) lib_ui_allowed_waits=([0-9]+) lib_ui_watched_waits=([0-9]+)")
  message(FATAL_ERROR "no ui_stats_pump line in:\n${o}")
endif()
set(pump ${CMAKE_MATCH_1})
if(pump GREATER 49)
  message(FATAL_ERROR "pump_ms_max is ${pump}, want < 50 (the catalogue count waited on the pump thread)")
endif()
if(NOT "${o}" MATCHES "view_reads panel_reads=([0-9]+) art_reads=([0-9]+) total_reads=([0-9]+)")
  message(FATAL_ERROR "no view_reads line in:\n${o}")
endif()
if(CMAKE_MATCH_3 LESS 1)
  message(FATAL_ERROR "the posted catalogue count never ran (total_reads=${CMAKE_MATCH_3})")
endif()
message(STATUS "song_total_async_ok pump_ms_max=${pump} total_reads=${CMAKE_MATCH_3}")
