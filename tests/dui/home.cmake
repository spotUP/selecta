# dui-home (DUI-P4-2): the Home view as shelves of tiles over the engine's HOME view source. -DSELECTA -DSONGS -DWORK
# The fixture library gets a seeded play history (oldest to newest: songs 3, 1, 5, 2, 6, 4; songs 3, 5, 6, 4 skipped).
# 1. "Recently played" holds the last 6 songs, newest first: 4 6 2 5 1 3 (checked in order from the ui_home dump).
# 2. "Jump back in" holds the albums of the skipped songs, newest first (engine rule: an unfinished play).
# 3. Each shelf scrolls on its own: the wheel over shelf 0 moves only shelf 0, over shelf 1 only shelf 1.
# 4. Activating tile 0 sends play to the engine: play_state 1 and songs_opened=1.
# 5. Revert check: a dump built with the order of the history reversed must fail the same assertion.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-home needs the sqlite3 command line tool to load the fixture library")
endif()
configure_file("${SONGS}/selecta-test.mod" "${WORK}/a.mod" COPYONLY)

set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
# Song 4 is the newest play and is a real file, so activating its tile can start the engine.
string(CONCAT seed
  "UPDATE songs SET key = '${WORK}/a.mod', path = '${WORK}/a.mod' WHERE id = 4; "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:3', 1000, 1); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:1', 2000, 0); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:5', 3000, 1); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:2', 4000, 0); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:6', 5000, 1); "
  "INSERT INTO history(key, played_at, skipped) VALUES('${WORK}/a.mod', 6000, 1)")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the history failed: ${err}")
endif()

function(home_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view home --canvas 700x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# Tile lines of one shelf, in order, as "key" values.
function(shelf_keys outvar out shelf)
  string(REGEX MATCHALL "shelf ${shelf} tile [0-9]+ [^|\n]*\\|[^|\n]*\\|" lines "${out}")
  set(keys "")
  foreach(l IN LISTS lines)
    string(REGEX REPLACE "^shelf [0-9]+ tile [0-9]+ [^|]*\\|([^|]*)\\|$" "\\1" k "${l}")
    list(APPEND keys "${k}")
  endforeach()
  set(${outvar} "${keys}" PARENT_SCOPE)
endfunction()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

function(shelf_scrolls out_s0 out_s1 out)
  if(NOT "${out}" MATCHES "ui_home id=desktop shelves=([0-9]+) shelf_scroll=([0-9]+)")
    message(FATAL_ERROR "no ui_home line in:\n${out}")
  endif()
  math(EXPR s0 "${CMAKE_MATCH_2} & 65535")
  math(EXPR s1 "(${CMAKE_MATCH_2} >> 16) & 65535")
  set(${out_s0} ${s0} PARENT_SCOPE)
  set(${out_s1} ${s1} PARENT_SCOPE)
endfunction()

set(recent_want "${WORK}/a.mod;fixture:6;fixture:2;fixture:5;fixture:1;fixture:3")

# 1 and 2. The shelves from the history.
home_run(idle "wait 1\n")
if(NOT "${idle}" MATCHES "ui_home id=desktop shelves=([0-9]+)" OR CMAKE_MATCH_1 LESS 2)
  message(FATAL_ERROR "fewer than 2 shelves in:\n${idle}")
endif()
shelf_keys(recent "${idle}" 0)
expect_eq("Recently played order" "${recent}" "${recent_want}")
shelf_keys(jump "${idle}" 1)
expect_eq("Jump back in albums" "${jump}" "Ünïcødé Álbum åäö;Monkey Island Theme (Extended Remaster Edition With A Very Long Title);Lotus Turbo Challenge;Strange Sounds from Beyond")
if(NOT idle MATCHES "shelf 0 tile 0 Recently played\\|" OR NOT idle MATCHES "shelf 1 tile 0 Jump back in\\|")
  message(FATAL_ERROR "shelf headings missing in:\n${idle}")
endif()
shelf_scrolls(a0 a1 "${idle}")
expect_eq("idle scroll shelf 0" "${a0}" 0)
expect_eq("idle scroll shelf 1" "${a1}" 0)

# 2b. Home is the start view itself: the shelves are in the dump of a run that replays no input at all, so the
# shell's start-view logic (not a later UI turn) selected HOME before the first frame was drawn.
home_run(first "")
if(NOT "${first}" MATCHES "ui_home id=desktop shelves=([0-9]+)" OR CMAKE_MATCH_1 LESS 2)
  message(FATAL_ERROR "Home shelves missing on the first frame in:\n${first}")
endif()

# 5. Revert check: the reversed order must not equal what the engine gave (the assertion is not vacuous).
set(reversed "fixture:3;fixture:1;fixture:5;fixture:2;fixture:6;${WORK}/a.mod")
if("${recent}" STREQUAL "${reversed}")
  message(FATAL_ERROR "revert check: the reversed history order matches, the order assertion cannot fail")
endif()

# 3. Independent scrolling. Shelf 0 band is y 172-418, shelf 1 starts at y 434 at this canvas; 700 wide makes both overflow.
home_run(s0only "move 400 300\nwheel -3 0\n")
shelf_scrolls(b0 b1 "${s0only}")
if(NOT b0 GREATER 0)
  message(FATAL_ERROR "wheel over shelf 0 did not scroll it: ${b0}")
endif()
expect_eq("shelf 1 after wheel over shelf 0" "${b1}" 0)
home_run(s1only "move 400 470\nwheel -3 0\n")
shelf_scrolls(c0 c1 "${s1only}")
if(NOT c1 GREATER 0)
  message(FATAL_ERROR "wheel over shelf 1 did not scroll it: ${c1}")
endif()
expect_eq("shelf 0 after wheel over shelf 1" "${c0}" 0)

# 4. Activation: tile 0 of shelf 0 plays the newest song through the engine.
dui_stat(state0 "${idle}" desktop play_state)
expect_eq("play_state before" "${state0}" 0)
home_run(played "tile 0\nwait 1\n")
dui_stat(state1 "${played}" desktop play_state)
expect_eq("play_state after activating tile 0" "${state1}" 1)
if(NOT "${played}" MATCHES "songs_opened=1")
  message(FATAL_ERROR "the activated song was not opened by the engine:\n${played}")
endif()
