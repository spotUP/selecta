# dui-lib-ui-guard (I906a): the debug UI-wait guard and pump_ms_max, through the real app entry point.
# The shell marks its loop thread with sel_library_watch(1) once the library is open. Opening the Home page makes the pump send
# library requests; every one that is not nowait is a guard hit (printed as library_ui_wait: on stderr, counted). Asserts:
# 1. the ui_stats_pump line exists with a pump_ms_max number (the longest pump_turn is measured);
# 2. lib_ui_guard_hits is 0 (I906c removed the art join, the Home run's last wait; a new wait fails here with its backtrace);
# 3. the run still exits 0 in the default log mode (the guard prints and counts, it does not abort).
# Release builds compile the guard out; this test then only checks pump_ms_max.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-lib-ui-guard needs the sqlite3 command line tool to load the fixture library")
endif()
configure_file("${SONGS}/selecta-test.mod" "${WORK}/a.mod" COPYONLY)
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()

file(WRITE "${WORK}/in.txt" "wait 1\n")
set(ENV{SELECTA_LIB_UI_GUARD} "log")
dui_run(o --db "${db}" --ui desktop --backend mem --format index8 --canvas 640x512 --clock fixed --stats --input "${WORK}/in.txt")
if(NOT "${o}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+) lib_ui_guard_hits=([0-9]+) ")
  message(FATAL_ERROR "no ui_stats_pump line (pump_ms_max not measured) in:\n${o}")
endif()
set(hits "${CMAKE_MATCH_2}")
if(BUILD_TYPE STREQUAL "Debug")
  # I906c: the art_load join was the one wait this Home run made; art now runs on a background batch, so the run makes no UI wait.
  # The guard itself is proved by engine-library-ui-guard (it forces a wait); this run must stay at 0 so a new wait fails here.
  if(NOT hits EQUAL 0)
    message(FATAL_ERROR "lib_ui_guard_hits is ${hits}: the Home run made a library wait on the UI thread:\n${o_err}")
  endif()
endif()
