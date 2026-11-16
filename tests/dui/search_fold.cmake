# dui-search-fold (I1406): search categories fold to their first 3 rows under a headline that shows the result count, with a Show all link.
# 1. Songs (fixture mod songs, more than 3) draws heading + 3 rows and a Show all link whose heading carries the count.
# 2. Clicking Show all draws every song row; the state is expanded.
# 3. A second search keeps Songs expanded (per session, no store); another click folds it again.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-search-fold needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
# The fixture has 2 mod songs; five more make the Songs category longer than the 3 rows a folded category draws.
set(extra "")
foreach(n RANGE 1 5)
  string(APPEND extra "INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(10${n}, 'fold:${n}', 'local', 'fold/${n}.mod', 'Fold Tune ${n}', 'Fold Artist', 'Fold Album', 'mod', 1993, 'Chiptune', 90000, 17000001${n});\n")
endforeach()
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${extra}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "adding the songs failed: ${err}")
endif()
set(mod_count 7)

function(fold_run tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x2600 --clock fixed --stats --input "${WORK}/${tag}.txt")
  set(${tag}_out "${o}\n${o_err}" PARENT_SCOPE)
endfunction()

function(expect out pattern what)
  if(NOT out MATCHES "${pattern}")
    message(FATAL_ERROR "${what}: no match for '${pattern}' in:\n${out}")
  endif()
endfunction()

set(typed "key /\nkey f\nkey o\nkey r\nkey m\nkey a\nkey t\nkey :\nkey m\nkey o\nkey d\nwait 600\n")
set(again "key backspace\nkey backspace\nkey backspace\nkey m\nkey o\nkey d\nwait 600\n")

# Folded: the Songs link exists, the headline names the count, the groups line counts the full result.
fold_run(folded "${typed}")
expect("${folded_out}" "search_fold 0 expanded=0 clicks=0 Songs \\(${mod_count}\\)\n" "Songs folded with its count on the headline")
expect("${folded_out}" "search groups songs=${mod_count} " "the groups line keeps the full count")
string(REGEX MATCH "facet=0 drawn=([0-9]+)" unused "${folded_out}")
set(drawn_folded "${CMAKE_MATCH_1}")

# Show all: every song row is drawn.
fold_run(opened "${typed}searchfold 0\nwait 100\n")
expect("${opened_out}" "search_fold 0 expanded=1 clicks=1 Songs" "Show all expands Songs")
string(REGEX MATCH "facet=0 drawn=([0-9]+)" unused "${opened_out}")
math(EXPR want "${drawn_folded} + ${mod_count} - 3")
if(NOT CMAKE_MATCH_1 EQUAL want)
  message(FATAL_ERROR "Show all must draw ${want} rows (folded ${drawn_folded}), got ${CMAKE_MATCH_1}")
endif()

# A second search in the same session keeps Songs expanded; the link folds it again.
fold_run(second "${typed}searchfold 0\nwait 100\n${again}")
expect("${second_out}" "search_fold 0 expanded=1 clicks=1 Songs" "the second search keeps Songs expanded")
fold_run(back "${typed}searchfold 0\nwait 100\nsearchfold 0\nwait 100\n")
expect("${back_out}" "search_fold 0 expanded=0 clicks=2 Songs" "the link folds Songs again")

# I1409: Down walks the visible rows across categories. Songs is folded to 3 rows: Down x3 stays in Songs, Down x4 reaches
# the next category (the 4th..7th Songs rows are hidden, not keyboard stops).
fold_run(k3 "${typed}key down\nkey down\nkey down\nwait 100\n")
expect("${k3_out}" "search cursor=[0-9]+ in=Songs " "three Downs stay in Songs")
fold_run(k4 "${typed}key down\nkey down\nkey down\nkey down\nwait 100\n")
expect("${k4_out}" "search cursor=[0-9]+ in=Albums " "the fourth Down moves into Albums, past the folded Songs rows")
