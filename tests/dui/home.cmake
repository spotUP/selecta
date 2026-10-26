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

# 6. Queue next (I247): right-click on a song tile opens its menu, the Play Next item sends QUEUE_PLAY_NEXT with the
# tile's song key (tile_queued counts it). Tile 0 of shelf 0 is a song at this canvas; the item sits at the click point.
# 7. Vertical overflow (I247): the shelves are taller than the main panel, so a wheel over the greeting block (not over
# tile cards) scrolls the whole content, home_scroll_y > 0, while the shelves' own horizontal scroll stays 0.
function(home_stat outvar out key)
  if(NOT "${out}" MATCHES "ui_home id=desktop [^\n]*${key}=([0-9]+)")
    message(FATAL_ERROR "no ${key} in the ui_home line of:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
home_run(probe "wait 1\n")
home_stat(q0 "${probe}" tile_queued)
expect_eq("tile_queued idle" "${q0}" 0)
home_stat(y0 "${probe}" scroll_y)
expect_eq("home_scroll_y idle" "${y0}" 0)
home_run(menu_only "move 100 300\nrclick 100 300\n")
dui_stat(m1 "${menu_only}" desktop menus)
if(NOT m1 GREATER 0)
  message(FATAL_ERROR "right-click on a song tile opened no menu (menus=${m1}) in:\n${menu_only}")
endif()
home_stat(q1 "${menu_only}" tile_queued)
expect_eq("tile_queued with the menu open only" "${q1}" 0)
home_run(queued "move 100 300\nrclick 100 300\nclick 110 336\nwait 1\n")
# The engine queue length (playlist_entries on the cmdline_runs line) is what Play Next must change, not a send count.
function(queue_len outvar out)
  if(NOT "${out}" MATCHES "playlist_entries=([0-9]+)")
    message(FATAL_ERROR "no playlist_entries in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
queue_len(ql0 "${menu_only}")
queue_len(ql1 "${queued}")
math(EXPR ql_grew "${ql1} - ${ql0}")
expect_eq("engine queue length after Play Next" "${ql_grew}" 1)
dui_stat(pl2 "${queued}" desktop play_state)
expect_eq("Play Next does not start playback" "${pl2}" 0)
home_run(vscroll "move 400 20\nwheel 0 -3\n")
home_stat(y1 "${vscroll}" scroll_y)
if(NOT y1 GREATER 0)
  message(FATAL_ERROR "wheel over the header block did not scroll the Home content vertically: ${y1}")
endif()
shelf_scrolls(v0 v1 "${vscroll}")
expect_eq("vertical wheel leaves shelf 0 alone" "${v0}" 0)
# I311: the same vertical wheel over tile cards scrolls the overflowing Home too (the cards cover most of the panel);
# the shelf's own horizontal scroll stays 0.
home_run(hover_cards "move 400 300\nwheel 0 -3\n")
home_stat(y2 "${hover_cards}" scroll_y)
if(NOT y2 GREATER 0)
  message(FATAL_ERROR "vertical wheel over tile cards did not scroll the overflowing Home: ${y2}")
endif()
shelf_scrolls(h0 h1 "${hover_cards}")
expect_eq("vertical wheel over cards leaves shelf 0 alone" "${h0}" 0)

# 8. Empty Home (I249): an empty library stays on Home and draws the empty state (ui_home empty=1) instead of the
# shell switching to the folder list; the Browse files action then switches the view source to browse. Revert check:
# with the empty-state code removed the dump says empty=0 and this assertion fails.
set(edb "${WORK}/empty.db")
dui_run(einit --db "${edb}" --ui desktop --backend mem --view home --canvas 700x600 --clock fixed)
function(empty_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg2")
  file(WRITE "${WORK}/in2.txt" "${input}")
  execute_process(COMMAND "${SELECTA}" --output null --size 80x25 --config-dir "${WORK}/cfg2" --db "${edb}" --ui desktop
    --backend mem --view home --canvas 700x600 --clock fixed --stats --input "${WORK}/in2.txt"
    OUTPUT_VARIABLE o ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "empty-library run exited ${rc}: ${err}")
  endif()
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
empty_run(e0 "wait 1\n")
home_stat(em0 "${e0}" empty)
expect_eq("empty library draws the empty state" "${em0}" 1)
if(NOT "${e0}" MATCHES "view_source=home ")
  message(FATAL_ERROR "an empty Home must stay on the Home source, not switch silently:\n${e0}")
endif()
empty_run(e1 "click 130 228\nwait 1\n")
home_stat(em1 "${e1}" empty)
expect_eq("Browse files leaves the empty state" "${em1}" 0)
if(NOT "${e1}" MATCHES "view_source=browse ")
  message(FATAL_ERROR "the Browse files click did not switch to the browse source:\n${e1}")
endif()

# 9. Shelves of the live library (I311), on a copy of the fixture db with two more songs: song 7 has no album and a
# skipped play, song 8 was added just now. Before the fix: an empty shelf kept its heading, Jump back in listed albums
# only, and New this week held only a Modland count line, so the base db reported 3 shelves and these tiles were absent.
# (a) Empty shelf: the base db has no song added in the last 7 days, so New this week is not a shelf at all.
if(NOT "${idle}" MATCHES "ui_home id=desktop shelves=2 ")
  message(FATAL_ERROR "an empty New this week shelf must not be drawn (expected shelves=2) in:\n${idle}")
endif()
set(db2 "${WORK}/library2.db")
file(COPY_FILE "${db}" "${db2}")
string(CONCAT seed2
  "INSERT INTO songs(id, key, source, path, name, artist, album, format, added) VALUES(7, 'fixture:7', 'local', 'fixture/7.mod', 'Albumless', 'Nobody', '', 'mod', 1700000007); "
  "INSERT INTO songs(id, key, source, path, name, artist, album, format, added) VALUES(8, 'fixture:8', 'local', 'fixture/8.mod', 'Fresh Song', 'Somebody', 'Fresh Album', 'mod', CAST(strftime('%s', 'now') AS INTEGER)); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:7', 7000, 1)")
execute_process(COMMAND "${SQLITE3}" "${db2}" "PRAGMA trusted_schema = 1; ${seed2}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the second library failed: ${err}")
endif()
function(home2_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db2}" --ui desktop --backend mem --view home --canvas 700x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
home2_run(two "wait 1\n")
# (b) Jump back in lists the albumless skipped song itself, newest first.
shelf_keys(jump2 "${two}" 1)
expect_eq("Jump back in with a song without an album" "${jump2}" "fixture:7;Ünïcødé Álbum åäö;Monkey Island Theme (Extended Remaster Edition With A Very Long Title);Lotus Turbo Challenge;Strange Sounds from Beyond")
# (c) New this week lists the song added today.
shelf_keys(fresh2 "${two}" 2)
expect_eq("New this week holds the song added today" "${fresh2}" "fixture:8")
if(NOT "${two}" MATCHES "shelf 2 tile 0 New this week\\|" OR NOT "${two}" MATCHES "ui_home id=desktop shelves=3 ")
  message(FATAL_ERROR "New this week is not the third shelf in:\n${two}")
endif()
# (d) A Home taller than the canvas: the wheel over tile cards scrolls it to the same limit as the wheel over the
# header block, and that limit is the bottom of the last shelf (home_max_y_q16 = content height minus panel height).
home2_run(top_max "move 400 20\nwheel 0 -200\n")
home_stat(ymax "${top_max}" scroll_y)
if(NOT ymax GREATER 0)
  message(FATAL_ERROR "Home with three shelves does not overflow a 600 high canvas: ${ymax}")
endif()
home2_run(card_max "move 400 300\nwheel 0 -200\n")
home_stat(ycard "${card_max}" scroll_y)
expect_eq("wheel over cards reaches the last shelf" "${ycard}" "${ymax}")

# ---- I314: a wheel notch on Home redraws the main area only, like Songs and Library (I310) ----
# The same input with and without one vertical notch must differ by less than the 700x600 canvas and draw no extra
# widgets: a full canvas clear plus every widget per notch is what made the scroll lag.
home_run(wnone "wait 1\nmove 400 20\nwait 1\n")
home_run(wnotch "wait 1\nmove 400 20\nwheel 0 -1\nwait 1\n")
home_stat(wn_y "${wnotch}" scroll_y)
if(wn_y LESS 1)
  message(FATAL_ERROR "one Home wheel notch did not scroll the content (scroll_y=${wn_y}):\n${wnotch}")
endif()
dui_stat(wn_t0 "${wnone}" desktop dirty_total)
dui_stat(wn_t1 "${wnotch}" desktop dirty_total)
dui_stat(wn_w0 "${wnone}" desktop widget_draws)
dui_stat(wn_w1 "${wnotch}" desktop widget_draws)
math(EXPR wn_px "${wn_t1} - ${wn_t0}")
math(EXPR wn_canvas "700 * 600")
message(STATUS "I314 home notch: ${wn_px} px of ${wn_canvas}, widget draws ${wn_w0} -> ${wn_w1}")
if(wn_px LESS 1 OR NOT wn_px LESS wn_canvas)
  message(FATAL_ERROR "one Home wheel notch redrew ${wn_px} pixels; it must redraw more than 0 and less than the ${wn_canvas} pixel canvas")
endif()
if(NOT wn_w1 EQUAL wn_w0)
  message(FATAL_ERROR "one Home wheel notch redrew widgets: ${wn_w0} before, ${wn_w1} after")
endif()
