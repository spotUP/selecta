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

# DUI-P4-6 groups (I256): one group per type with the fixture's counts, drawn from the engine's grouped rows. The expected
# distinct albums and artists are counted by the database from the same fixture, not typed in.
execute_process(COMMAND "${SQLITE3}" "${db}" "SELECT count(DISTINCT album) || ' ' || count(DISTINCT artist) FROM songs WHERE format = 'mod'"
  OUTPUT_VARIABLE distinct OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE " " ";" distinct "${distinct}")
list(GET distinct 0 want_albums)
list(GET distinct 1 want_artists)
expect("${burst_out}" "search groups songs=${mod_count} albums=${want_albums} artists=${want_artists} facet=0 drawn=([0-9]+)\n" "one group per type with the fixture counts")
math(EXPR want_all "3 + ${mod_count} + ${want_albums} + ${want_artists}")
string(REGEX MATCH "facet=0 drawn=([0-9]+)" unused "${burst_out}")
if(NOT CMAKE_MATCH_1 EQUAL want_all)
  message(FATAL_ERROR "All draws 3 headings and every entry: expected ${want_all} rows, got ${CMAKE_MATCH_1}")
endif()

# A chip click changes the drawn rows: Albums leaves its heading and the albums only; Songs, its heading and the songs.
function(chip_click label want_facet want_drawn)
  # The stats keep the labels of earlier pages; the search chips are the last ones drawn.
  string(REGEX MATCHALL "label (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) ink [-0-9]+ [-0-9]+ text ${label}\n" found "${burst_out}")
  if(NOT found)
    message(FATAL_ERROR "no '${label}' chip label in the stats")
  endif()
  list(GET found -1 last)
  string(REGEX MATCH "label (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)" unused "${last}")
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  search_run(chip_${label} "${typed}wait 600\nclick ${cx} ${cy}\nwait 100\n")
  expect("${chip_${label}_out}" "search groups songs=${mod_count} albums=${want_albums} artists=${want_artists} facet=${want_facet} drawn=${want_drawn}\n" "clicking ${label}")
  if(chip_${label}_hash STREQUAL burst_hash)
    message(FATAL_ERROR "clicking ${label} left the frame unchanged")
  endif()
endfunction()
math(EXPR songs_drawn "1 + ${mod_count}")
math(EXPR albums_drawn "1 + ${want_albums}")
math(EXPR artists_drawn "1 + ${want_artists}")
chip_click("Songs" 1 ${songs_drawn})
chip_click("Albums" 2 ${albums_drawn})
chip_click("Artists" 3 ${artists_drawn})

# Chips before a query (I256 follow-up): the Search page draws its own facet chips with an empty query, and a click on one
# keeps the facet. The Home chips (All, Songs, Composers, Playlists) must not be what is drawn there.
search_run(empty "key /\nwait 100\n")
# The stats keep the labels of the Home page drawn first; the chips of the page showing now are the last four drawn.
string(REGEX REPLACE "^.*text All\n" "" empty_tail "${empty_out}")
expect("${empty_tail}" "text Songs\n.*text Albums\n.*text Artists\n" "the empty Search page draws All, Songs, Albums, Artists")
if(empty_tail MATCHES "text Composers\n" OR empty_tail MATCHES "text Playlists\n")
  message(FATAL_ERROR "the empty Search page draws the Home chips:\n${empty_tail}")
endif()
string(REGEX MATCHALL "label (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) ink [-0-9]+ [-0-9]+ text Songs\n" found "${empty_out}")
list(GET found -1 last)
string(REGEX MATCH "label (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)" unused "${last}")
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
search_run(empty_click "key /\nwait 100\nclick ${cx} ${cy}\nwait 100\n")
expect("${empty_click_out}" "search groups songs=0 albums=0 artists=0 facet=1 drawn=" "clicking Songs without a query keeps the facet")
if(empty_click_hash STREQUAL empty_hash)
  message(FATAL_ERROR "clicking Songs without a query left the frame unchanged")
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
