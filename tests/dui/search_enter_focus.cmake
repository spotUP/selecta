# dui-search-enter-focus (I1009): Enter in the desktop Search field runs the search and the field keeps the keyboard.
# Type a query, press Enter, type one more character: the query must grow ("format:mod" -> "format:modx") and the search
# sentinel must draw the longer query. Before the fix Enter dropped the focus and the extra key went nowhere.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-search-enter-focus needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
file(WRITE "${WORK}/enter.txt"
  "key /\nkey f\nkey o\nkey r\nkey m\nkey a\nkey t\nkey :\nkey m\nkey o\nkey d\nkey enter\nkey x\nwait 600\n")
dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
  --input "${WORK}/enter.txt")
set(out "${o}\n${o_err}")
if(NOT out MATCHES "search draws [0-9]+ rows [0-9]+ query format:modx\n")
  message(FATAL_ERROR "a key typed after Enter must extend the query (field keeps focus); got:\n${out}")
endif()
