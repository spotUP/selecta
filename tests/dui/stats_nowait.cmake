# dui-stats-nowait (I958): the per-frame engine stats read never asks the library worker.
# -DSELECTA -DWORK -DSDL_BUILT
# Design: thoughts/shared/research/2026-11-01-ui-thread-library-waits.md section 1 (existing async patterns) and section 3 (guard).
# Before: sel_engine_stats_get called sel_library_rating_pending on every desktop frame (about a dozen call sites per frame); nowait only
# skips a BUSY worker, a free one was a blocking round trip each time (74 of 320 non-idle main-thread samples). Now the count is the
# writer's atomic (sel_library_rating_pending_count), a plain load.
# Driven through the desktop UI's live SDL loop. SELECTA_LIB_PENDING_SLOW_MS = 300 slows every pending read in the library worker.
# Asserts:
# 1. pump_ms_max < 50, turn_ms_max < 50 and lib_ui_watched_waits == 0: no frame waited on the worker;
# 2. sentinel: rating_pending == 2, the two fixture pending rows were counted and served without a read request.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-stats-nowait needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
execute_process(COMMAND "${SQLITE3}" "${db}"
  "INSERT INTO pending_push(kind, key, payload, created) VALUES('rating', 'fixture:1', '5', 1), ('rating', 'fixture:2', '4', 1);"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding pending_push failed: ${err}")
endif()
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_LIB_PENDING_SLOW_MS} 300)
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
set(watched ${CMAKE_MATCH_4})
if(pump GREATER 49)
  message(FATAL_ERROR "pump_ms_max is ${pump}, want < 50 (the stats read waited on the library worker)")
endif()
# A nowait request that got a free call lock is counted by neither lib_ui_watched_waits nor lib_ui_wait_ms (it is one statement away),
# so the old per-frame round trip shows only as the wall time of the desk_wait turn: a 300 ms slowed read makes turn_ms_max >= 300.
if(NOT "${o}" MATCHES "turn_ms_max=([0-9]+)")
  message(FATAL_ERROR "no turn_ms_max in:\n${o}")
endif()
if(CMAKE_MATCH_1 GREATER 49)
  message(FATAL_ERROR "turn_ms_max is ${CMAKE_MATCH_1}, want < 50 (a desk_wait turn waited on the library worker)")
endif()
if(NOT watched EQUAL 0)
  message(FATAL_ERROR "lib_ui_watched_waits is ${watched}, want 0")
endif()
if(NOT "${o}" MATCHES "rating_pending=([0-9]+)" OR NOT CMAKE_MATCH_1 EQUAL 2)
  message(FATAL_ERROR "sentinel: rating_pending is not 2 in:\n${o}")
endif()
message(STATUS "stats_nowait_ok pump_ms_max=${pump} watched_waits=${watched}")
