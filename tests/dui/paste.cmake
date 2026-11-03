# dui-paste (I999): Command+V (Control+V elsewhere) pastes the clipboard into a desktop text field.
# Johan 2026-11-02: "i tried to paste in both search fields in mac selecta but i cant paste in them".
# All six one-line fields share field_edit (desktop_ui.c); this drives the top Search field through the real key path.
# 1. --clipboard stands in for the system clipboard (a test never touches the real pasteboard). Pasting `format:mod`
#    plus a second line puts only the first line in the field; the search runs and draws the fixture's mod count.
# 2. The stats line `pastes` counts the paste: proof the paste branch ran, not typed keys.
# 3. Command+V with an empty clipboard leaves the field empty and counts no paste.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
file(READ "${HERE}/fixture_library.sql" fixture)
string(REGEX MATCHALL "INSERT INTO songs\\([^\n]*'mod', [0-9]+," mods "${fixture}")
list(LENGTH mods mod_count)
if(mod_count LESS 1)
  message(FATAL_ERROR "the fixture has no mod songs to count")
endif()

file(WRITE "${WORK}/paste.txt" "key /\nkey ctrl+v\nwait 600\n")
file(REMOVE_RECURSE "${WORK}/cfg")
dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
  --clipboard "format:mod\nsecond line" --input "${WORK}/paste.txt")
if(NOT "${o}" MATCHES "search draws [1-9][0-9]* rows ${mod_count} query format:mod\n")
  message(FATAL_ERROR "the pasted query must search and draw ${mod_count} mod rows:\n${o}")
endif()
if(NOT "${o}" MATCHES "\npastes 1\n")
  message(FATAL_ERROR "the paste branch must count one paste:\n${o}")
endif()

file(WRITE "${WORK}/empty.txt" "key /\nkey ctrl+v\nwait 600\n")
file(REMOVE_RECURSE "${WORK}/cfg")
dui_run(e --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
  --input "${WORK}/empty.txt")
if(NOT "${e}" MATCHES "\npastes 0\n" OR NOT "${e}" MATCHES "query \n")
  message(FATAL_ERROR "an empty clipboard must paste nothing:\n${e}")
endif()
message(STATUS "dui-paste: pasted query drew ${mod_count} mod rows, 1 paste counted")
