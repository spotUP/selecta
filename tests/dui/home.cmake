# dui-home (DUI-P4-2): the Home view as shelves of tiles over the engine's HOME view source. -DSELECTA -DSONGS -DWORK
# The fixture library gets a seeded play history (oldest to newest: songs 3, 1, 5, 2, 6, 4; songs 3, 5, 6, 4 skipped).
# 1. "Recently played" holds the last 6 songs, newest first: 4 6 2 5 1 3 (checked in order from the ui_home dump).
# 2. "Jump back in" holds the albums of the skipped songs, newest first (engine rule: an unfinished play).
# 3. Shelf buttons (I607): the wheel never moves a shelf sideways; round previous/next buttons show on the hovered shelf only and page it.
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
  dui_run(o --db "${db}" --ui desktop --backend mem --view home --canvas 700x856 --clock fixed --stats --input "${WORK}/in.txt")
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
# I482: a song tile (not an album) carries its artist as the last dump field, like an album tile (I476). Song 4 is Zoe Bjork's.
if(NOT "${idle}" MATCHES "shelf 0 tile [0-9]+ [^\n|]*\\|[^\n|]*a\\.mod\\|[^\n|]*\\|Zoë Björk\n")
  message(FATAL_ERROR "the song tile of song 4 must carry its artist as its last dump field in:\n${idle}")
endif()
# I485: the artist is drawn on its own line, so a tile title is the name alone. Dump line: heading|key|title|artist. Song 4 is
# "Cafe Del Mar - Cafe Mix" by Zoe Bjork (title field ends at the | before the artist), the Lotus album tile is by Chris Huelsbeck.
if(NOT "${idle}" MATCHES "a\\.mod\\|Cafe Del Mar – Café Mix\\|Zoë Björk\n")
  message(FATAL_ERROR "the song tile title must be the song name alone (no \" - artist\" suffix) in:\n${idle}")
endif()
if(NOT "${idle}" MATCHES "shelf 1 tile [0-9]+ Jump back in\\|Lotus Turbo Challenge\\|Lotus Turbo Challenge\\|Chris Huelsbeck\n")
  message(FATAL_ERROR "the album tile title must be the album name alone (no \" - artist\" suffix) in:\n${idle}")
endif()
expect_eq("idle scroll shelf 0" "${a0}" 0)
expect_eq("idle scroll shelf 1" "${a1}" 0)

# 1b. I469: the album tiles of "Jump back in" draw the library cover of their albums row (fixture albums 1 to 3 carry a
# cover, the Unicode album has none and keeps generated art). The shelf_album_art_draws counter proves the cover path ran;
# without the album lookup in the art provider (art_find) every album tile falls back to generated art and it stays 0.
dui_stat(album_art "${idle}" desktop shelf_album_art_draws)
expect_eq("shelf album tiles drawn with a library cover" "${album_art}" 3)

# I477: tile art is clipped to the card radius. A drawn cover's top-left corner pixel is the card fill behind it (the art
# does not square off the rounded card), and its centre pixel is the cover, not the card.
if(NOT "${idle}" MATCHES "\ntile_art corner=([0-9a-f]+) centre=([0-9a-f]+) fill=([0-9a-f]+)\n")
  message(FATAL_ERROR "no tile_art line (a shelf tile cover was drawn) in:\n${idle}")
endif()
set(art_corner "${CMAKE_MATCH_1}")
set(art_centre "${CMAKE_MATCH_2}")
set(art_fill "${CMAKE_MATCH_3}")
expect_eq("tile art corner pixel is the card fill" "${art_corner}" "${art_fill}")
if("${art_centre}" STREQUAL "${art_fill}")
  message(FATAL_ERROR "tile art centre pixel equals the card fill ${art_fill}; the cover was not drawn")
endif()

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

# 3. Shelf buttons (I607). The wheel never scrolls a shelf sideways, vertical or horizontal; the page scrolls instead.
# Round previous/next buttons show only on the shelf under the pointer; previous is hidden at the shelf start; a click on
# next moves that shelf (and only that one) by one page of covers and then shows previous; a click on previous returns.
# Shelf 0 band is y 172-418, shelf 1 starts at y 434 at this canvas; 700 wide makes both overflow. The dump's shelf_nav
# lines are what the last paint drew, so a button that was not painted reads 0.
function(home_stat outvar out key)
  if(NOT "${out}" MATCHES "ui_home id=desktop [^\n]*${key}=([0-9]+)")
    message(FATAL_ERROR "no ${key} in the ui_home line of:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
function(shelf_nav out s what)
  if(NOT "${out}" MATCHES "shelf_nav ${s} prev=([01]) next=([01]) prev_at=(-?[0-9]+),(-?[0-9]+) next_at=(-?[0-9]+),(-?[0-9]+) labels=Previous,Next")
    message(FATAL_ERROR "${what}: no shelf_nav ${s} line in:\n${out}")
  endif()
  set(nav_prev ${CMAKE_MATCH_1} PARENT_SCOPE)
  set(nav_next ${CMAKE_MATCH_2} PARENT_SCOPE)
  set(nav_prev_x ${CMAKE_MATCH_3} PARENT_SCOPE)
  set(nav_prev_y ${CMAKE_MATCH_4} PARENT_SCOPE)
  set(nav_next_x ${CMAKE_MATCH_5} PARENT_SCOPE)
  set(nav_next_y ${CMAKE_MATCH_6} PARENT_SCOPE)
endfunction()
shelf_nav("${idle}" 0 "idle")
expect_eq("previous button with the pointer elsewhere" "${nav_prev}" 0)
expect_eq("next button with the pointer elsewhere" "${nav_next}" 0)
home_run(over0 "move 400 444\n")
shelf_nav("${over0}" 0 "hover shelf 0")
expect_eq("previous button hidden at the shelf start" "${nav_prev}" 0)
expect_eq("next button shown while hovering the shelf" "${nav_next}" 1)
set(nx ${nav_next_x})
set(ny ${nav_next_y})
shelf_nav("${over0}" 1 "hover shelf 0")
expect_eq("shelf 1 next button while hovering shelf 0" "${nav_next}" 0)
# The wheel moves the page, never the shelf.
home_run(wheel_v "move 400 444\nwheel 0 -3\n")
shelf_scrolls(w0 w1 "${wheel_v}")
expect_eq("shelf 0 after a vertical wheel over it" "${w0}" 0)
home_stat(wy "${wheel_v}" scroll_y)
if(NOT wy GREATER 0)
  message(FATAL_ERROR "the vertical wheel over a shelf did not scroll the page: scroll_y=${wy}")
endif()
home_run(wheel_h "move 400 444\nwheel -3 0\nwheel 3 0\n")
shelf_scrolls(h0 h1 "${wheel_h}")
expect_eq("shelf 0 after a horizontal wheel over it" "${h0}" 0)
expect_eq("shelf 1 after a horizontal wheel over shelf 0" "${h1}" 0)
# A click on next moves shelf 0 by one page; shelf 1 stays; previous now shows.
home_run(page1 "move 400 444\nclick ${nx} ${ny}\n")
shelf_scrolls(p0 p1 "${page1}")
if(NOT p0 GREATER 0)
  message(FATAL_ERROR "the next button did not move shelf 0: ${p0} in:\n${page1}")
endif()
expect_eq("shelf 1 after the next button of shelf 0" "${p1}" 0)
shelf_nav("${page1}" 0 "after next")
expect_eq("previous button shown once the shelf moved" "${nav_prev}" 1)
set(px ${nav_prev_x})
set(py ${nav_prev_y})
home_run(back "move 400 444\nclick ${nx} ${ny}\nclick ${px} ${py}\n")
shelf_scrolls(b0 b1 "${back}")
expect_eq("shelf 0 after next then previous" "${b0}" 0)
dui_stat(played_by_button "${page1}" desktop play_state)
expect_eq("a button click does not activate the tile under it" "${played_by_button}" 0)

# 3b. Shelf button motion (I630). The buttons fade in on hover, a click eases the shelf one page on, and the button under the
# pointer grows to about 1.1x (scale 72090 of 65536) and shrinks back on leave. The run ends by finishing every tween, so the
# motion is read with `navsnap` lines taken mid-tween: a value strictly between its ends proves the tween ran (a pop or a hard
# jump has none).
function(expect_between what v lo hi)
  if(NOT v GREATER lo OR NOT v LESS hi)
    message(FATAL_ERROR "${what}: ${v} is not strictly between ${lo} and ${hi}")
  endif()
endfunction()
string(CONCAT motion_in
  "move 400 444\nwait 32\nnavsnap\nwait 32\nnavsnap\nwait 400\nnavsnap\n"
  "move ${nx} ${ny}\nwait 32\nnavsnap\nwait 400\nnavsnap\n"
  "click ${nx} ${ny}\nwait 48\nnavsnap\nwait 48\nnavsnap\nwait 600\nnavsnap\n"
  "move 400 444\nwait 32\nnavsnap\nwait 400\nnavsnap\n"
  "move 5 5\nwait 32\nnavsnap\nwait 400\nnavsnap\n")
home_run(motion "${motion_in}")
string(REGEX MATCHALL "nav snap alpha=[-0-9]+ scale_prev=[0-9]+ scale_next=[0-9]+ scroll=[0-9]+" snaps "${motion}")
list(LENGTH snaps n_snaps)
expect_eq("navsnap lines in the dump" "${n_snaps}" 12)
set(i 0)
foreach(s IN LISTS snaps)
  string(REGEX REPLACE "nav snap alpha=([-0-9]+) scale_prev=([0-9]+) scale_next=([0-9]+) scroll=([0-9]+)" "\\1;\\2;\\3;\\4" f "${s}")
  list(GET f 0 sn${i}_alpha)
  list(GET f 1 sn${i}_prev)
  list(GET f 2 sn${i}_next)
  list(GET f 3 sn${i}_scroll)
  math(EXPR i "${i} + 1")
endforeach()
# Fade: rising over frames after the pointer enters the shelf, full when it ends, back to zero after the pointer left.
expect_between("button opacity 32 ms after hover" ${sn0_alpha} 0 65536)
expect_between("button opacity 64 ms after hover" ${sn1_alpha} ${sn0_alpha} 65536)
expect_eq("button opacity once the fade is over" "${sn2_alpha}" 65536)
expect_between("button opacity 32 ms after the pointer left the shelf" ${sn10_alpha} 0 65536)
expect_eq("button opacity once faded out" "${sn11_alpha}" 0)
# What the paint drew, as the alpha byte (I630 flicker): mid-fade partly opaque, and fully opaque once the fade is over. A Q16
# 65536 shifted down to 256 wraps to alpha 0, so the buttons vanished the moment their fade-in ended.
string(REGEX MATCHALL "nav snap [^\n]* painted=[0-9]+" painted_snaps "${motion}")
list(GET painted_snaps 0 pa0)
list(GET painted_snaps 2 pa2)
string(REGEX REPLACE ".* painted=([0-9]+)" "\\1" pa0 "${pa0}")
string(REGEX REPLACE ".* painted=([0-9]+)" "\\1" pa2 "${pa2}")
expect_between("painted button alpha 32 ms after hover" ${pa0} 0 255)
expect_eq("painted button alpha once the fade is over" "${pa2}" 255)
# Grow: the hovered next button scales up while the previous one stays, and it returns on leave.
expect_eq("next button scale before it is hovered" "${sn2_next}" 65536)
expect_between("next button scale 32 ms after the pointer entered it" ${sn3_next} 65536 72090)
expect_eq("next button scale while hovered" "${sn4_next}" 72090)
expect_eq("previous button scale while next is hovered" "${sn4_prev}" 65536)
expect_between("next button scale 32 ms after the pointer left it" ${sn8_next} 65536 72090)
expect_eq("next button scale after leaving it" "${sn9_next}" 65536)
# Easing: the click passes intermediate positions and settles exactly where the instant jump of section 3 ended.
expect_eq("shelf 0 before the click" "${sn4_scroll}" 0)
expect_between("shelf 0 48 ms into the scroll" ${sn5_scroll} 0 ${p0})
expect_between("shelf 0 96 ms into the scroll" ${sn6_scroll} ${sn5_scroll} ${p0})
expect_eq("shelf 0 once the eased scroll settled (one page)" "${sn7_scroll}" "${p0}")

# 3c. The buttons belong to the cover row under the pointer (I630 fix). The shelf title row and the title/artist lines under the
# covers do not trigger them, a hover that is then left alone (no further move) keeps them at full opacity, and a wheel after the
# hover keeps the drawn shelf and the hovered shelf the same one.
shelf_nav("${over0}" 1 "hover shelf 0")
set(cover1_y ${nav_next_y})
math(EXPR title_y "${ny} - 100")
math(EXPR caption_y "${ny} + 100")
function(snap_alphas out idx)
  string(REGEX MATCHALL "nav snap alpha=[-0-9]+ scale_prev=[0-9]+ scale_next=[0-9]+ scroll=[0-9]+ alpha1=[-0-9]+" all "${out}")
  list(GET all ${idx} one)
  string(REGEX REPLACE "nav snap alpha=([-0-9]+) .* alpha1=([-0-9]+)" "\\1;\\2" f "${one}")
  list(GET f 0 a0)
  list(GET f 1 a1)
  set(snap_a0 ${a0} PARENT_SCOPE)
  set(snap_a1 ${a1} PARENT_SCOPE)
endfunction()
home_run(rowt "move 400 ${title_y}\nwait 400\nnavsnap\nmove 400 ${caption_y}\nwait 400\nnavsnap\n")
snap_alphas("${rowt}" 0)
expect_eq("shelf 0 buttons while the pointer is on the shelf title row" "${snap_a0}" 0)
snap_alphas("${rowt}" 1)
expect_eq("shelf 0 buttons while the pointer is on the caption lines" "${snap_a0}" 0)
home_run(rowh "move 400 ${ny}\nwait 400\nnavsnap\nwait 800\nnavsnap\nmove 400 ${cover1_y}\nwait 400\nnavsnap\n")
snap_alphas("${rowh}" 1)
expect_eq("shelf 0 buttons stay after the fade when the pointer rests" "${snap_a0}" 65536)
snap_alphas("${rowh}" 2)
expect_eq("shelf 0 buttons after the pointer moved to shelf 1" "${snap_a0}" 0)
expect_eq("shelf 1 buttons on the shelf 1 cover row" "${snap_a1}" 65536)
home_run(roww "move 400 ${cover1_y}\nwait 400\nwheel 0 -3\nwait 400\nnavsnap\n")
snap_alphas("${roww}" 0)
if(snap_a0 GREATER 0 AND snap_a1 GREATER 0)
  message(FATAL_ERROR "both shelves draw buttons after a wheel")
endif()

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
home_run(probe "wait 1\n")
home_stat(q0 "${probe}" tile_queued)
expect_eq("tile_queued idle" "${q0}" 0)
home_stat(y0 "${probe}" scroll_y)
expect_eq("home_scroll_y idle" "${y0}" 0)
home_run(menu_only "move 100 444\nrclick 100 444\n")
dui_stat(m1 "${menu_only}" desktop menus)
if(NOT m1 GREATER 0)
  message(FATAL_ERROR "right-click on a song tile opened no menu (menus=${m1}) in:\n${menu_only}")
endif()
home_stat(q1 "${menu_only}" tile_queued)
expect_eq("tile_queued with the menu open only" "${q1}" 0)
home_run(queued "move 100 444\nrclick 100 444\nclick 110 480\nwait 1\n")
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
home_run(hover_cards "move 400 444\nwheel 0 -3\n")
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
empty_run(e1 "click 130 284\nwait 1\n")
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

# 10. I471: two albums with the same name by different artists keep their own covers on the Jump back in shelf. Song 9 is a
# skipped play on a second "Lotus Turbo Challenge" (album 5, artist Other Artist, a copy of album 3's cover); fixture album 1
# carries the same name by Chris Huelsbeck. Before the fix the shelf grouped by album name alone (one Lotus tile, so album 5
# never loaded: shelf_album_art_draws 3, no thumb144 row for album 5) and art_find matched the name only (lowest id wins).
set(db3 "${WORK}/library3.db")
file(COPY_FILE "${db}" "${db3}")
string(CONCAT seed3
  "INSERT INTO songs(id, key, source, path, name, artist, album, format, added) VALUES(9, 'fixture:9', 'local', 'fixture/9.mod', 'Lotus Again', 'Other Artist', 'Lotus Turbo Challenge', 'mod', 1700000009); "
  "INSERT INTO albums(id, name, artist, year, source) VALUES(5, 'Lotus Turbo Challenge', 'Other Artist', 1993, 'local'); "
  "INSERT INTO artwork(album_id, kind, data) SELECT 5, 'cover', data FROM artwork WHERE album_id = 3 AND kind = 'cover'; "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:9', 7500, 1)")
execute_process(COMMAND "${SQLITE3}" "${db3}" "PRAGMA trusted_schema = 1; ${seed3}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the third library failed: ${err}")
endif()
set(db4 "${WORK}/library4.db")
file(COPY_FILE "${db3}" "${db4}") # runs consume the seeded history, so each run below gets its own copy
file(COPY_FILE "${db3}" "${WORK}/library5.db")
file(COPY_FILE "${db3}" "${WORK}/library6.db")
file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/in.txt" "wait 1\n")
dui_run(same --db "${db3}" --ui desktop --backend mem --view home --canvas 1000x800 --clock fixed --stats --input "${WORK}/in.txt")
dui_stat(same_draws "${same}" desktop shelf_album_art_draws)
expect_eq("same-named albums each draw a cover (Lotus x2, Monkey Island, Strange Sounds)" "${same_draws}" 4)
# I476: the dump's last tile field is the artist line a tile draws; each Lotus album tile carries its own album artist, an album
# without a known artist carries none (an empty field, no placeholder).
foreach(who "Other Artist" "Chris Huelsbeck")
  if(NOT "${same}" MATCHES "shelf [0-9]+ tile [0-9]+ [^\n|]*\\|Lotus Turbo Challenge\\|[^\n|]*\\|${who}\n")
    message(FATAL_ERROR "the Lotus album tile by ${who} must carry that artist as its last dump field in:\n${same}")
  endif()
endforeach()
if("${same}" MATCHES "\\|Album\n")
  message(FATAL_ERROR "a Home tile must not carry the placeholder 'Album' as its artist in:\n${same}")
endif()
execute_process(COMMAND "${SQLITE3}" "${db3}" "SELECT count(*) FROM artwork WHERE album_id = 5 AND kind = 'thumb144'"
  OUTPUT_VARIABLE own_thumb RESULT_VARIABLE rc)
string(STRIP "${own_thumb}" own_thumb)
expect_eq("the second Lotus album loaded its own cover" "${own_thumb}" 1)

# ---- I314: a wheel notch on Home redraws the main area only, like Songs and Library (I310) ----
# The same input with and without one vertical notch must differ by less than the 700x800 canvas and draw no extra
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
math(EXPR wn_canvas "700 * 800")
message(STATUS "I314 home notch: ${wn_px} px of ${wn_canvas}, widget draws ${wn_w0} -> ${wn_w1}")
if(wn_px LESS 1 OR NOT wn_px LESS wn_canvas)
  message(FATAL_ERROR "one Home wheel notch redrew ${wn_px} pixels; it must redraw more than 0 and less than the ${wn_canvas} pixel canvas")
endif()
if(NOT wn_w1 EQUAL wn_w0)
  message(FATAL_ERROR "one Home wheel notch redrew widgets: ${wn_w0} before, ${wn_w1} after")
endif()

# ---- I370 (DUI-P4-2): a small library fills Home from the library and hides the empty shelves ----
# A copy of the fixture db cut to 4 songs with no play history: Recently played, Jump back in and New this week are all
# empty, so the engine's fill shelf "From your library" is the only shelf, holding the 4 songs newest-added first. The
# sentinel is the shelf itself (heading text and the 4 tile keys in the dump come only from the fill path); the other
# three headings are asserted absent. Revert check: without the fill the dump has no shelf and shelves=0.
set(db3 "${WORK}/library3.db")
file(COPY_FILE "${db}" "${db3}")
execute_process(COMMAND "${SQLITE3}" "${db3}" "PRAGMA trusted_schema = 1; DELETE FROM history; DELETE FROM songs WHERE id > 4"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "cutting the library to 4 songs failed: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/in.txt" "wait 1\n")
dui_run(small --db "${db3}" --ui desktop --backend mem --view home --canvas 700x600 --clock fixed --stats --input "${WORK}/in.txt")
if(NOT "${small}" MATCHES "ui_home id=desktop shelves=1 ")
  message(FATAL_ERROR "a 4-song library must show exactly one shelf (the fill), empty shelves hidden, in:\n${small}")
endif()
if(NOT "${small}" MATCHES "shelf 0 tile 0 From your library\\|")
  message(FATAL_ERROR "the fill shelf heading is missing in:\n${small}")
endif()
shelf_keys(fill "${small}" 0)
expect_eq("fill shelf holds the 4 songs, newest added first" "${fill}" "${WORK}/a.mod;fixture:3;fixture:2;fixture:1")
foreach(empty "Recently played" "Jump back in" "New this week")
  if("${small}" MATCHES "${empty}")
    message(FATAL_ERROR "empty shelf '${empty}' was drawn for a 4-song library:\n${small}")
  endif()
endforeach()

# ---- I479 (DUI-P4-2): the "Your composers" shelf ----
# A copy of the 4-song library with composers: Huelsbeck has 3 songs, Galway 1. The fill shelf is shelf 0 and the composers shelf
# is shelf 1, tile 0 = the composer with the most songs (key = canon, title = display name). Activating tile 4 (flat index: the 4
# fill tiles come first) must open that composer's songs on the Library page (drill line: display|3). The library with no composer
# (db3 above) draws no such shelf. Revert check: without the shelf the dump has shelves=1 and no tile 4.
set(db7 "${WORK}/library7.db")
file(COPY_FILE "${db3}" "${db7}")
string(CONCAT seed7
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:1', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:2', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:3', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:3', 'rob hubbard', 'Rob Hubbard')")
execute_process(COMMAND "${SQLITE3}" "${db7}" "PRAGMA trusted_schema = 1; ${seed7}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the composers library failed: ${err}")
endif()
if("${small}" MATCHES "Your composers")
  message(FATAL_ERROR "a library without composers drew the Your composers shelf:\n${small}")
endif()
function(comp_run outvar script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${script}")
  dui_run(o --db "${db7}" --ui desktop --backend mem --view home --canvas 1000x1400 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
comp_run(comps "wait 1\n")
if(NOT "${comps}" MATCHES "ui_home id=desktop shelves=2 ")
  message(FATAL_ERROR "expected the fill shelf and Your composers (shelves=2) in:\n${comps}")
endif()
if(NOT "${comps}" MATCHES "shelf 1 tile 0 Your composers\\|")
  message(FATAL_ERROR "the Your composers heading is not shelf 1 in:\n${comps}")
endif()
shelf_keys(comp_keys "${comps}" 1)
expect_eq("Your composers tiles, most songs first" "${comp_keys}" "chris huelsbeck;rob hubbard")
if(NOT "${comps}" MATCHES "Chris Huelsbeck")
  message(FATAL_ERROR "the composer display name is not the tile title in:\n${comps}")
endif()
comp_run(comp_open "tile 4\nwait 1\n")
if(NOT "${comp_open}" MATCHES "collection kind=composer [^\n]* of=3 ")
  message(FATAL_ERROR "activating the composer tile did not open its 3 songs in:\n${comp_open}")
endif()

# 10b. I473: the album is its name and artist when it is opened too. Shelf 0 holds 7 tiles, so tile 7 is the first tile of Jump
# back in: "Lotus Turbo Challenge - Other Artist" (the dump above). Activating it runs the album search; the query source then
# lists that album's songs only (fixture:9). Before the fix it ran album:"Lotus Turbo Challenge" alone and listed 3 songs
# (this album's one and the other album's two). The run uses a fresh copy of the seeded db (a run consumes the home history).
function(album_run outvar db_path script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${script}")
  dui_run(o --db "${db_path}" --ui desktop --backend mem --view home --canvas 1000x1400 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
function(expect_album_rows out what rows key)
  dui_stat_line_rows("${out}" got_rows)
  expect_eq("${what}: songs listed" "${got_rows}" "${rows}")
  if(NOT "${out}" MATCHES "view_source=query [^\n]*cursor_key=${key}\n")
    message(FATAL_ERROR "${what}: the query view does not start at ${key}:\n${out}")
  endif()
endfunction()
function(dui_stat_line_rows out var)
  if(NOT "${out}" MATCHES "\nview_rows=([0-9]+)\n")
    message(FATAL_ERROR "no view_rows line in:\n${out}")
  endif()
  set(${var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
album_run(act "${db4}" "tile 7\nexec VIEW_SOURCE query\nwait 1\n")
expect_album_rows("${act}" "the Other Artist shelf tile opens its own album" 1 "fixture:9")

# 10c. I473: the same for Search album rows. album:Lotus lists both albums; the Albums group has one row per name and artist
# (rows: heading, 4 songs, heading, 2 albums, ...). Row 5 is the Chris Huelsbeck album (2 songs: fixture:1 first) and row 6 the
# Other Artist album (1 song). Before the fix the row key carried the name alone: each opened 3 songs.
set(search_open "exec LIBRARY_SEARCH album:Lotus\nexec VIEW_SOURCE search\nwait 1\nexec VIEW_CURSOR")
album_run(srch_other "${WORK}/library5.db" "${search_open} 6\nwait 1\nexec VIEW_ACTIVATE\nwait 1\n")
expect_album_rows("${srch_other}" "the Other Artist Search album row opens its own album" 1 "fixture:9")
album_run(srch_first "${WORK}/library6.db" "${search_open} 5\nwait 1\nexec VIEW_ACTIVATE\nwait 1\n")
expect_album_rows("${srch_first}" "the Chris Huelsbeck Search album row opens its own album" 2 "fixture:1")

# 11. I478: the quick-pick grid at the top of Home. The engine lists the most recent distinct albums of the play history (at
# most 8, newest first); the UI lays them out 4 columns wide when a cell keeps its minimum width, else 2; a tile opens its
# album through the shelf tile path. The fixture history holds 4 albums (Unicode, Monkey Island, Lotus, Strange Sounds).
function(quick_keys outvar out)
  string(REGEX MATCHALL "(^|\n)quick [0-9]+ cols=[0-9]+ [^|\n]*\\|" lines "${out}")
  set(${outvar} "${lines}" PARENT_SCOPE)
endfunction()
function(quick_count outvar out)
  quick_keys(l "${out}")
  list(LENGTH l n)
  set(${outvar} ${n} PARENT_SCOPE)
endfunction()
function(quick_run outvar db_path canvas script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${script}")
  dui_run(o --db "${db_path}" --ui desktop --backend mem --view home --canvas ${canvas} --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
function(quick_db name)
  set(qd "${WORK}/${name}.db")
  dui_run(qi --db "${qd}" --ui desktop --backend mem --view home --canvas 700x600 --clock fixed --stats)
  execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${qd}" INPUT_FILE "${HERE}/fixture_library.sql" RESULT_VARIABLE rc ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "loading the fixture failed: ${err}")
  endif()
  execute_process(COMMAND "${SQLITE3}" "${qd}" "PRAGMA trusted_schema = 1; ${ARGN}" RESULT_VARIABLE rc ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "seeding ${name} failed: ${err}")
  endif()
  set(QUICK_DB "${qd}" PARENT_SCOPE)
endfunction()
# Fewer than 8: three played albums (songs 1, 2, 3 oldest to newest) give exactly 3 tiles, newest first.
quick_db(few "INSERT INTO history(key, played_at, skipped) VALUES('fixture:1', 1000, 0); INSERT INTO history(key, played_at, skipped) VALUES('fixture:2', 2000, 0); INSERT INTO history(key, played_at, skipped) VALUES('fixture:3', 3000, 0)")
set(few_db "${QUICK_DB}")
quick_run(few_out "${few_db}" 700x800 "wait 1\n")
quick_count(few_n "${few_out}")
expect_eq("quick picks with 3 played albums" "${few_n}" 3)
if(NOT "${few_out}" MATCHES "quick 0 cols=2 Strange Sounds from Beyond\\|[^\n]*\nquick 1 cols=2 Monkey Island Theme")
  message(FATAL_ERROR "quick picks must list the newest album first in:\n${few_out}")
endif()
# Wide canvas: 4 columns.
quick_run(wide_out "${few_db}" 1280x800 "wait 1\n")
if(NOT "${wide_out}" MATCHES "quick 0 cols=4 ")
  message(FATAL_ERROR "a wide canvas must lay the quick picks out in 4 columns:\n${wide_out}")
endif()
# At most 8: twelve distinct played albums give 8 tiles, the newest first (album Q12).
set(many "")
foreach(i RANGE 1 8)
  math(EXPR sid "100 + ${i}")
  math(EXPR at "10000 + ${i}")
  string(APPEND many "INSERT INTO songs(id, key, source, path, name, artist, album, format, added) VALUES(${sid}, 'q:${i}', 'local', 'q/${i}.mod', 'Song ${i}', 'Artist', 'Quick Album ${i}', 'mod', 1700000100); INSERT INTO history(key, played_at, skipped) VALUES('q:${i}', ${at}, 0); ")
endforeach()
quick_db(many "${many} INSERT INTO history(key, played_at, skipped) VALUES('fixture:1', 1000, 0); INSERT INTO history(key, played_at, skipped) VALUES('fixture:2', 2000, 0); INSERT INTO history(key, played_at, skipped) VALUES('fixture:3', 3000, 0)")
quick_run(many_out "${QUICK_DB}" 1280x800 "wait 1\n")
quick_count(many_n "${many_out}")
expect_eq("quick picks are capped at 8" "${many_n}" 8)
if(NOT "${many_out}" MATCHES "quick 0 cols=4 Quick Album 8\\|")
  message(FATAL_ERROR "the newest played album must be quick pick 0 in:\n${many_out}")
endif()
# No history: no grid.
quick_run(none_out "${edb}" 700x800 "wait 1\n")
quick_count(none_n "${none_out}")
expect_eq("quick picks without history" "${none_n}" 0)
# Activating quick pick 1 (Monkey Island, songs 2 and 6) opens that album's two songs, exactly like its shelf tile (I473 path).
quick_run(qact "${few_db}" 1000x1400 "quicktile 1\nexec VIEW_SOURCE query\nwait 1\n")
expect_album_rows("${qact}" "quick pick 1 opens its album (Monkey Island: fixture:2 and fixture:6)" 2 "fixture:2")

# I490: the pointer over quick-pick tile 1 lights that card like a hovered shelf card: the dump carries quick_hover with the
# lit fill (the panel mixed to the surface token), and no such line while no quick tile is hovered.
quick_run(qidle "${few_db}" 1000x1400 "wait 1\n")
if("${qidle}" MATCHES "\nquick_hover ")
  message(FATAL_ERROR "quick_hover reported with no pointer over a quick tile:\n${qidle}")
endif()
quick_run(qhov "${few_db}" 1000x1400 "quickhover 1\nwait 1\n")
if(NOT "${qhov}" MATCHES "\nquick_hover fill=([0-9a-f]+)\n")
  message(FATAL_ERROR "hovering quick tile 1 lit no card (no quick_hover line):\n${qhov}")
endif()
set(qfill "${CMAKE_MATCH_1}")
quick_run(qhov_off "${few_db}" 1000x1400 "quickhover 1\nmove 5 5\nwait 1\n")
if("${qhov_off}" MATCHES "\nquick_hover ")
  message(FATAL_ERROR "quick tile 1 stayed lit after the pointer left:\n${qhov_off}")
endif()

# 12. I499: the live data path. The fixture library above is hand-inserted; a real library is built by a scan of a folder
# of mp3s with embedded cover art (tests/dui/fixtures/art-music: three albums, two tracks each, one APIC picture per
# file). The desktop UI scans it through the real entry point (a dropped folder runs LIBRARY_SCAN), the scan must leave one
# artwork row per album, and after one skipped play of a scanned song Home shows the Quick picks grid with that album and
# draws its library cover (shelf_album_art_draws > 0), not generated art.
set(live "${WORK}/live")
file(REMOVE_RECURSE "${live}")
file(MAKE_DIRECTORY "${live}")
file(COPY "${HERE}/fixtures/art-music" DESTINATION "${live}")
set(live_db "${live}/library.db")
file(WRITE "${live}/scan.txt" "wait 1\ndrop ${live}/art-music\nwait 5\n")
dui_run(live_scan --start-dir "${live}/art-music" --db "${live_db}" --ui desktop --backend mem --view home --canvas 1280x800
  --clock fixed --stats --input "${live}/scan.txt")
function(live_sql outvar sql)
  execute_process(COMMAND "${SQLITE3}" "${live_db}" "${sql}" RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_VARIABLE o)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "live db query failed: ${err}")
  endif()
  string(STRIP "${o}" o)
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
live_sql(live_songs "SELECT COUNT(*) FROM songs")
live_sql(live_art "SELECT COUNT(*) FROM artwork WHERE kind = 'cover'")
expect_eq("scan of the art-music folder: songs" "${live_songs}" 6)
expect_eq("scan of the art-music folder: one cover per album" "${live_art}" 3)
live_sql(live_key "SELECT key FROM songs WHERE album = 'Iron Garden' ORDER BY key LIMIT 1")
live_sql(live_seed "INSERT INTO history(key, played_at, skipped) VALUES('${live_key}', strftime('%s','now'), 1); SELECT 1")
file(WRITE "${live}/home.txt" "wait 2\n")
dui_run(live_home --db "${live_db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
  --input "${live}/home.txt")
quick_count(live_quick "${live_home}")
expect_eq("live-scanned library: Quick picks tiles" "${live_quick}" 1)
if(NOT "${live_home}" MATCHES "quick 0 cols=4 Iron Garden\\|Iron Garden\\|Mono Lake")
  message(FATAL_ERROR "the played scanned album must be quick pick 0 in:\n${live_home}")
endif()
dui_stat(live_covers "${live_home}" desktop shelf_album_art_draws)
if(live_covers LESS 1)
  message(FATAL_ERROR "a scanned album with an embedded cover drew generated art on Home (shelf_album_art_draws=${live_covers}):\n${live_home}")
endif()
