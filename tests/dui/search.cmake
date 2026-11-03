# dui-search (DUI-P4-6): the desktop Search with live results, and the typed query drawn in the field (I239). -DSELECTA -DSONGS -DWORK -DGOLDEN
# 1. Typing `format:mod` in one burst gives exactly the fixture's count of mod songs (counted from the fixture file).
# 2. STAT_LIBRARY_SEARCHES grows by the debounced count: 1 per pause, not per character; Enter right after typing adds none
#    when the debounce already sent the query.
# 3. The dump shows the typed query (I239) and the search sentinel proves the search view drew; a golden hash pins the frame.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# The fan-out asks every searchable source (Modules.pl, Internet Archive since I1024a). Block all network so each From
# section fails at once and the golden frame never depends on a live service or its timing.
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
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

set(SEARCH_CANVAS 1280x800)
function(search_run tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas ${SEARCH_CANVAS} --clock fixed --stats
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
# I1019e: with no source chip the fan-out adds one "From <name>" section per searchable source after the library groups; each
# is its heading plus its rows (a state row while the answer is pending or failed). Count them from the dump's section lines.
# I1024e: with five From sections the 800 px frame clips the last rows, so the drawn count is read from a canvas tall enough
# for every row (the golden frame above stays 1280x800).
set(SEARCH_CANVAS 1280x1400)
search_run(tall "${typed}wait 600\n")
set(SEARCH_CANVAS 1280x800)
string(REGEX MATCHALL "search_section [0-9]+ rows=[0-9]+" from_sections "${tall_out}")
set(from_drawn 0)
foreach(sec IN LISTS from_sections)
  string(REGEX MATCH "rows=([0-9]+)" unused "${sec}")
  math(EXPR from_drawn "${from_drawn} + 1 + ${CMAKE_MATCH_1}")
endforeach()
math(EXPR want_all "${want_all} + ${from_drawn}")
string(REGEX MATCH "facet=0 drawn=([0-9]+)" unused "${tall_out}")
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
# The From sections hold online songs, so the Songs facet keeps them; Albums and Artists drop them.
math(EXPR songs_drawn "1 + ${mod_count} + ${from_drawn}")
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

# Johan 2026-11-02: "i cant scroll the search results". Every listed result must be reachable: with more hits than the body under
# the facet chips holds, a wheel scrolls the list. A copy of the fixture gets 30 songs that all match `scrollsong`; the engine sends
# a heading and 16 rows, taller than the body at 1280x400. Before the wheel the offset is 0 with a positive maximum; after a wheel
# past the end the offset is the maximum, the scroll path counted the notches, and a song row is drawn that was not drawn before.
set(sdb "${WORK}/scroll.db")
file(COPY_FILE "${db}" "${sdb}")
set(ins "")
foreach(i RANGE 1 30)
  string(APPEND ins "INSERT INTO songs(key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES('scroll:${i}', 'local', 'scroll/${i}.mod', 'Scrollsong ${i}', 'Scroll Artist', 'Scroll Album', 'mod', 1993, 'Chiptune', 60000, 1700000000);\n")
endforeach()
file(WRITE "${WORK}/scroll.sql" "${ins}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${sdb}" INPUT_FILE "${WORK}/scroll.sql" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "adding the scroll songs failed: ${err}")
endif()
set(sq "key /\nkey s\nkey c\nkey r\nkey o\nkey l\nkey l\nkey s\nkey o\nkey n\nkey g\nwait 600\n")
function(scroll_run tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${sdb}" --ui desktop --backend mem --view library --canvas 1280x400 --clock fixed --stats --input "${WORK}/${tag}.txt")
  string(REGEX MATCHALL "searchrow [0-9]+ [-0-9]+ [-0-9]+ [0-9]+ [0-9]+ key=[^\n]*" rows "${o}")
  set(keys "")
  foreach(r IN LISTS rows)
    string(REGEX REPLACE ".* key=" "" k "${r}")
    list(APPEND keys "${k}")
  endforeach()
  set(${tag}_out "${o}" PARENT_SCOPE)
  set(${tag}_keys "${keys}" PARENT_SCOPE)
endfunction()
scroll_run(scroll0 "${sq}")
if(NOT "${scroll0_out}" MATCHES "search scroll y=0 max=([0-9]+) wheels=0\n" OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "at 1280x400 the 16 scroll hits must overflow the results body (offset 0, maximum above 0):\n${scroll0_out}")
endif()
scroll_run(scroll1 "${sq}move 700 300\nwheel 0 -400\nwait 200\n")
if(NOT "${scroll1_out}" MATCHES "search scroll y=([0-9]+) max=([0-9]+) wheels=([0-9]+)\n" OR CMAKE_MATCH_3 LESS 1 OR NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "a wheel on the Search page must scroll the results to the end:\n${scroll1_out}")
endif()
# The end of the list holds the From sections (one per searchable source, I1024a added Internet Archive), so the rows drawn
# at the very end may all be From rows. Three notches (16 px each, rows are 32 px) from the top must bring in a song row that was hidden.
scroll_run(scroll2 "${sq}move 700 300\nwheel 0 -3\nwait 200\n")
if(NOT "${scroll2_out}" MATCHES "search scroll y=([0-9]+) max=[0-9]+ wheels=([0-9]+)\n" OR CMAKE_MATCH_1 LESS 1 OR CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "three wheel notches on the Search page must scroll the results:\n${scroll2_out}")
endif()
set(newrow "")
foreach(k IN LISTS scroll2_keys)
  list(FIND scroll0_keys "${k}" at)
  if(at EQUAL -1)
    set(newrow "${k}")
  endif()
endforeach()
if(newrow STREQUAL "")
  message(FATAL_ERROR "the scrolled Search page draws no song row the unscrolled page did not (before: ${scroll0_keys}; after three notches: ${scroll2_keys}):\n${scroll2_out}")
endif()

# I1012 (Johan 2026-11-02 Mac): Back from a page opened out of a Search result did not return to the Search page with what it showed. The
# Search page is a history entry like the others: leaving it keeps the query, the facet chip and the scroll offset in the entry, and Back
# puts them back (the offset waits for the results to be sent again). With Songs picked and the list scrolled to the end, a door page and
# Back must show the Search page again with the same query text, the same facet and the same offset. The route line is the sentinel
# that the history walked back (cur=search, pos 0 of 2 entries).
string(REGEX MATCHALL "label (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) ink [-0-9]+ [-0-9]+ text Songs\n" found "${scroll0_out}")
list(GET found -1 last)
string(REGEX MATCH "label (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)" unused "${last}")
math(EXPR bx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR by "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(sfacet "click ${bx} ${by}\nwait 100\nmove 700 300\nwheel 0 -400\nwait 200\n")
scroll_run(hist0 "${sq}${sfacet}")
if(NOT "${hist0_out}" MATCHES "search scroll y=([0-9]+) max=([0-9]+) wheels=([0-9]+)\n" OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "the Songs facet list must scroll before the history step:\n${hist0_out}")
endif()
set(want_y "${CMAKE_MATCH_1}")
scroll_run(hist1 "${sq}${sfacet}door 3\nwait 100\nkey alt+left\nwait 600\n")
if(NOT "${hist1_out}" MATCHES "route route_calls=[0-9]+ cur=search n=([0-9]+) pos=([0-9]+) ")
  message(FATAL_ERROR "Back from the door page must land on the Search entry of the history:\n${hist1_out}")
endif()
math(EXPR back_pos "${CMAKE_MATCH_2} + 2")
if(NOT back_pos EQUAL CMAKE_MATCH_1)
  message(FATAL_ERROR "Back must step one entry behind the door page (n=${CMAKE_MATCH_1} pos=${CMAKE_MATCH_2}):\n${hist1_out}")
endif()
# The restored offset is the end of the list, where only From sections are drawn since I1024a; the rows counter counts library song
# rows only, so the query text is the check here and the groups line below proves rows are drawn.
if(NOT "${hist1_out}" MATCHES "search draws [1-9][0-9]* rows [0-9]+ query scrollsong\n")
  message(FATAL_ERROR "Back to Search must show the query it was left with:\n${hist1_out}")
endif()
if(NOT "${hist1_out}" MATCHES "search groups songs=[0-9]+ albums=[0-9]+ artists=[0-9]+ facet=1 drawn=[1-9]")
  message(FATAL_ERROR "Back to Search must keep the Songs facet and its results:\n${hist1_out}")
endif()
if(NOT "${hist1_out}" MATCHES "search scroll y=${want_y} max=")
  message(FATAL_ERROR "Back to Search must restore the scroll offset ${want_y}:\n${hist1_out}")
endif()

# I1012 (Johan 2026-11-03 Mac, reopened): the search field keeps the keyboard focus through query, facet click and wheel, and it
# swallowed the history keys (field_edit has no edit for them, so desk_key returned before the bindings). A single click on a Search
# song row only selects and a double click plays in place, so neither opens a page; the swallow is the bug. With the field focused,
# Alt+Left must walk back one entry, and Alt+Right must walk forward to Search again with the query, facet and offset.
scroll_run(hist2 "${sq}${sfacet}key alt+left\nwait 600\n")
if(NOT "${hist2_out}" MATCHES "route route_calls=[0-9]+ cur=([a-z_]+) n=([0-9]+) pos=([0-9]+) " OR CMAKE_MATCH_1 STREQUAL "search")
  message(FATAL_ERROR "Alt+Left with the search field focused must leave Search for the previous history entry:\n${hist2_out}")
endif()
math(EXPR back_pos "${CMAKE_MATCH_3} + 2")
if(NOT back_pos EQUAL CMAKE_MATCH_2)
  message(FATAL_ERROR "Alt+Left with the search field focused must step one entry back (n=${CMAKE_MATCH_2} pos=${CMAKE_MATCH_3}):\n${hist2_out}")
endif()
scroll_run(hist2b "${sq}${sfacet}key alt+left\nwait 300\nkey alt+right\nwait 600\n")
if(NOT "${hist2b_out}" MATCHES "route route_calls=[0-9]+ cur=search n=([0-9]+) pos=([0-9]+) ")
  message(FATAL_ERROR "Alt+Left then Alt+Right with the search field focused must land on Search again:\n${hist2b_out}")
endif()
math(EXPR last_pos "${CMAKE_MATCH_2} + 1")
if(NOT last_pos EQUAL CMAKE_MATCH_1 OR NOT "${hist2b_out}" MATCHES "nav page=search nav_clicks=0 nav_keys=2 "
   OR NOT "${hist2b_out}" MATCHES "facet=1 drawn=[1-9]" OR NOT "${hist2b_out}" MATCHES "search scroll y=${want_y} max=")
  message(FATAL_ERROR "Alt+Right back to Search must be the last entry with both keys counted, the Songs facet and offset ${want_y}:\n${hist2b_out}")
endif()
# Search, door page, Alt+Left (Search refocuses the field), Alt+Right: the walk forward lands on the door page again.
scroll_run(hist3 "${sq}${sfacet}door 3\nwait 100\nkey alt+left\nwait 300\nkey alt+right\nwait 600\n")
if(NOT "${hist3_out}" MATCHES "route route_calls=[0-9]+ cur=([a-z_]+) n=([0-9]+) pos=([0-9]+) " OR CMAKE_MATCH_1 STREQUAL "search")
  message(FATAL_ERROR "Alt+Right with the search field focused must walk forward to the door page again (not search):\n${hist3_out}")
endif()
math(EXPR last_pos "${CMAKE_MATCH_3} + 1")
if(NOT last_pos EQUAL CMAKE_MATCH_2)
  message(FATAL_ERROR "Alt+Right must end on the last history entry (n=${CMAKE_MATCH_2} pos=${CMAKE_MATCH_3}):\n${hist3_out}")
endif()
