# dui-search (DUI-P4-6): the desktop Search with live results, and the typed query drawn in the field (I239). -DSELECTA -DSONGS -DWORK -DGOLDEN
# 1. Typing `format:mod` in one burst gives exactly the fixture's count of mod songs (counted from the fixture file).
# 2. STAT_LIBRARY_SEARCHES grows by the debounced count: 1 per pause, not per character; Enter right after typing adds none
#    when the debounce already sent the query.
# 3. The dump shows the typed query (I239) and the search sentinel proves the search view drew; a golden hash pins the frame.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-search needs the sqlite3 command line tool to load the fixture library")
endif()
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

function(search_run tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
    --input "${WORK}/${tag}.txt")
  set(out "${o}\n${o_err}")
  dui_stat(hash "${o}" desktop canvas_hash)
  if(NOT "${o}" MATCHES "library_screen opens=[0-9]+ searches=([0-9]+)")
    message(FATAL_ERROR "no library_screen line in:\n${o}")
  endif()
  set(libs "${CMAKE_MATCH_1}")
  set(${tag}_out "${out}" PARENT_SCOPE)
  set(${tag}_hash "${hash}" PARENT_SCOPE)
  set(${tag}_libs "${libs}" PARENT_SCOPE)
endfunction()

function(expect out pattern what)
  if(NOT out MATCHES "${pattern}")
    message(FATAL_ERROR "${what}: no match for '${pattern}' in:\n${out}")
  endif()
endfunction()

set(typed "key /\nkey f\nkey o\nkey r\nkey m\nkey a\nkey t\nkey :\nkey m\nkey o\nkey d\n")

# One burst, one pause: exactly one search; its result count is the fixture's mod count; the query is drawn.
search_run(burst "${typed}wait 600\n")
if(NOT burst_libs EQUAL 1)
  message(FATAL_ERROR "one burst of typing must send 1 library search (got ${burst_libs})")
endif()
expect("${burst_out}" "search draws [1-9][0-9]* rows ${mod_count} query format:mod\n" "search sentinel with the typed query")

# Two bursts separated by a pause: two searches.
search_run(twice "${typed}wait 600\nkey /\nkey m\nkey o\nkey d\nwait 600\n")
if(NOT twice_libs EQUAL 2)
  message(FATAL_ERROR "two bursts must send 2 library searches (got ${twice_libs})")
endif()

# Enter right after typing searches at once, and the pause after it adds none.
search_run(enter "${typed}key enter\nwait 600\n")
if(NOT enter_libs EQUAL 1)
  message(FATAL_ERROR "Enter then a pause must send 1 library search (got ${enter_libs})")
endif()

# No pause, no search: the debounce has not elapsed.
search_run(none "${typed}")
if(NOT none_libs EQUAL 0)
  message(FATAL_ERROR "typing without a pause must send no search yet (got ${none_libs})")
endif()
if(none_hash STREQUAL burst_hash)
  message(FATAL_ERROR "the search results frame equals the frame before the debounce")
endif()

set(gfile "${GOLDEN}/desktop/search-rgba32-1280x800@1.hash")
if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
  file(WRITE "${gfile}" "${burst_hash}\n")
endif()
if(NOT EXISTS "${gfile}")
  message(FATAL_ERROR "missing golden ${gfile} (hash ${burst_hash}); create it with SELECTA_GOLDEN_UPDATE=1")
endif()
file(STRINGS "${gfile}" want LIMIT_COUNT 1)
if(NOT burst_hash STREQUAL want)
  message(FATAL_ERROR "search hash ${burst_hash} differs from golden ${want} (${gfile})")
endif()
