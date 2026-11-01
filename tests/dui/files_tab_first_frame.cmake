# dui-files-tab-first-frame (I910): the Files tab (sidebar Local Files row) draws its files list on the first frame and never blocks the UI
# thread while the files load. -DSELECTA -DWORK -DSDL_BUILT
# Bug (Johan, Mac, real library.db): Browse > Local Files > Files first showed the empty Playlists page with a beachball; the files came later.
# Two causes, both through the real desktop UI entry:
#   1. desk_route presented the page before desk_route_enter set the collection target, and draw_playlists with no target draws the Playlists list;
#   2. cmd_COLLECTION_OPEN built the page (6+ library queries) on the pump thread, each a watched wait.
# Driven through the desktop UI's live SDL loop (timed click, no harness read waits, so pump_ms_max is the real pump).
# SELECTA_LIB_COLLECTION_SLOW_MS=300 makes every collection query take 0.3 s in the library worker (the real db made them seconds). Asserts:
# 1. pllist draws=0: no frame of the Playlists list page was drawn on the way to the Files page;
# 2. lib_ui_watched_waits=0 and pump_ms_max < 50: the pump never waited for the slowed queries;
# 3. the posted read ran and was taken (the page shows the data once the worker frees): collection kind=local shown=N of=N with N the sqlite3 count.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-files-tab-first-frame needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%';"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "keying the fixture songs failed: ${err}")
endif()
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "SELECT COUNT(*) FROM songs WHERE source = 'local';"
  RESULT_VARIABLE rc OUTPUT_VARIABLE want ERROR_VARIABLE err)
string(STRIP "${want}" want)
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(c0 --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x800 --input "${WORK}/idle.txt")
if(NOT "${c0}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=2 ")
  message(FATAL_ERROR "no Local Files sidebar row in:\n${c0}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SOCKET} "/tmp/selecta-dui-files-tab.sock")
set(ENV{SELECTA_LIB_COLLECTION_SLOW_MS} 300)
set(ENV{SELECTA_LIB_UI_GUARD} "log")
file(REMOVE_RECURSE "${WORK}/cfg")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_SDL_TEST_TIMED} "m${cx},${cy}@1500 d1,${cx},${cy}@1550 u1,${cx},${cy}@1600 q@5000")
# dui_run adds --output null, whose loop deliberately waits for posted reads (sel_engine_pump_reads) and would inflate pump_ms_max, so run
# the shell without it: the live SDL loop is the one that must not wait.
execute_process(COMMAND "${SELECTA}" --config-dir "${WORK}/cfg" --db "${db}" --ui desktop --backend sdl --view home --canvas 1280x800 --clock fixed --stats
  OUTPUT_VARIABLE o ERROR_VARIABLE o_err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${o_err}")
endif()
if(NOT "${o}" MATCHES "pllist draws=([0-9]+)\n")
  message(FATAL_ERROR "no pllist line in:\n${o}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL 0)
  message(FATAL_ERROR "the Playlists list page was drawn ${CMAKE_MATCH_1} time(s) on the way to the Files page:\n${o}")
endif()
if(NOT "${o}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+) lib_ui_guard_hits=([0-9]+) lib_ui_allowed_waits=([0-9]+) lib_ui_watched_waits=([0-9]+)")
  message(FATAL_ERROR "no ui_stats_pump line in:\n${o}")
endif()
set(pump ${CMAKE_MATCH_1})
set(watched ${CMAKE_MATCH_4})
if(NOT watched EQUAL 0)
  message(FATAL_ERROR "the pump waited ${watched} time(s) on library reads while the Files page loaded:\n${o}")
endif()
if(pump GREATER 49)
  message(FATAL_ERROR "pump_ms_max is ${pump}, want < 50:\n${o}")
endif()
if(NOT "${o}" MATCHES "collection kind=local [^\n]*shown=${want} of=${want}")
  message(FATAL_ERROR "the Files page did not show the ${want} local songs after the read:\n${o}")
endif()
message(STATUS "dui-files-tab-first-frame ok (pump_ms_max=${pump} songs=${want})")
