# dui-start-home (I594): with no --view the desktop UI starts on Home and draws it. -DSELECTA -DSONGS -DWORK
# The sidebar marks the Home door at start (page == PG_HOME), but the main area only entered Home mode when a door,
# a key or --view ran desk_page_apply. A bare `selecta --ui desktop` (the AGA rig run) showed the marked door over an
# empty main area. Startup now enters Home through the same path as the door.
# Asserts, with the real entry point, a fixture library with a play history and NO --view:
# 1. the dump holds the ui_home line with at least 2 shelves (Home was drawn);
# 2. the shelf "Recently Played" has tiles (the HOME source is live, not the browse source).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-start-home needs the sqlite3 command line tool to load the fixture library")
endif()
configure_file("${SONGS}/selecta-test.mod" "${WORK}/a.mod" COPYONLY)
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:3', 1000, 0); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:1', 2000, 0); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:5', 3000, 1)")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the history failed: ${err}")
endif()

file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/in.txt" "wait 1\n")
dui_run(o --db "${db}" --ui desktop --backend mem --format index8 --canvas 640x512 --clock fixed --stats --input "${WORK}/in.txt")
if(NOT "${o}" MATCHES "ui_home id=desktop shelves=([0-9]+)" OR CMAKE_MATCH_1 LESS 2)
  message(FATAL_ERROR "no --view: Home was not drawn at start (need ui_home with at least 2 shelves) in:\n${o}")
endif()
if(NOT "${o}" MATCHES "shelf 0 tile 0 Recently Played\\|")
  message(FATAL_ERROR "no --view: the Recently Played shelf has no tiles in:\n${o}")
endif()
