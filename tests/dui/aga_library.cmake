# dui-aga-library (I678k; design 2026-10-29-desktop-library-architecture.md section 12): the Your Library sidebar and the collection page
# on the Amiga bitmap tier (--canvas 640x256 --ydiv 2), through the real desktop UI entry. -DSELECTA -DSONGS -DWORK
# Fixture fixture_sidebar_mixed.sql: all 15, Playlists 2, Albums 3, Artists 3, Composers 3, Sources 1, Local Files 1 (the Mac counts).
# 1. Key K steps the chip (All, Playlists, Albums, Artists, Composers, Sources, Local Files, All): `sidelib items` equals the Mac count
#    at every step and the sentinel `sidechips cycles` grows by one per press.
# 2. The rows carry a one-letter kind mark (`sidechips marks` > 0) and the view is Compact list only (`sidelib_density view=0`).
# 3. A click on the Mix row opens page=collection with of=2 (the Mac count); the header art is 64 px at 640 wide and 0 at 320 wide;
#    the header is the flat panel colour (flat > 0).
# 4. Backspace goes back to Home, Shift+Backspace forward to the collection page.
# Revert check: with side_view_of returning d->side_view, or the K handler removed, the density and cycles assertions fail.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-aga-library needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x1400 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_sidebar_mixed.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()
function(items_of out)
  if(NOT "${out}" MATCHES "sidelib draws=[0-9]+ rows=[0-9]+ items=([0-9]+) ")
    message(FATAL_ERROR "no sidelib line in:\n${out}")
  endif()
  set(got_items "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
function(field_of out dump pattern what)
  if(NOT "${dump}" MATCHES "${pattern}")
    message(FATAL_ERROR "no ${what} in:\n${dump}")
  endif()
  set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 640x256 --ydiv 2 --zoom fit)

# 1 and 2. The K chip cycle.
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(c0 ${common} --input "${WORK}/idle.txt")
items_of("${c0}")
expect_eq("all items" "${got_items}" "15")
field_of(density "${c0}" "sidelib_density view=([0-9]+) " "sidelib_density line")
expect_eq("the bitmap tier offers the Compact list only" "${density}" "0")
field_of(marks "${c0}" "sidechips [^\n]* marks=([0-9]+)" "marks")
if(marks LESS 1)
  message(FATAL_ERROR "no kind mark was drawn on the bitmap tier:\n${c0}")
endif()
set(counts "2;3;3;3;1;1;15")
set(keys "")
set(prev_cycles 0)
foreach(k RANGE 0 6)
  string(APPEND keys "key k\nwait 100\n")
  file(WRITE "${WORK}/k${k}.txt" "${keys}")
  dui_run(k${k} ${common} --input "${WORK}/k${k}.txt")
  list(GET counts ${k} want)
  items_of("${k${k}}")
  expect_eq("items after ${k} K presses + 1" "${got_items}" "${want}")
  field_of(cycles "${k${k}}" "sidechips [^\n]* cycles=([0-9]+)" "cycles")
  math(EXPR cycles_want "${k} + 1")
  expect_eq("side_chip_cycles after K press ${cycles_want}" "${cycles}" "${cycles_want}")
endforeach()

# 3. Open the Mix playlist; 640 wide has 64 px header art, 320 wide none. Each open touches Recents and moves the rows, so the Mix
# row is read from an idle run right before the run that clicks it.
file(WRITE "${WORK}/idlek.txt" "key k\nwait 100\n")
function(mix_click out w)
  dui_run(idle_${w} --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas ${w}x256 --ydiv 2 --zoom fit --input "${WORK}/idlek.txt")
  if(NOT "${idle_${w}}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=[0-9]+ Mix\n")
    message(FATAL_ERROR "no Mix row at ${w} wide in:\n${idle_${w}}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "(${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2) / 2") # pointer rows are canvas rows, the dump is in square pixels (ydiv 2)
  set(${out} "key k\nwait 100\nclick ${cx} ${cy}\nwait 100\n" PARENT_SCOPE)
endfunction()
function(run_at out w script)
  mix_click(clk ${w})
  file(WRITE "${WORK}/run_${w}.txt" "${clk}${script}")
  dui_run(r_${w} --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas ${w}x256 --ydiv 2 --zoom fit --input "${WORK}/run_${w}.txt")
  set(${out} "${r_${w}}" PARENT_SCOPE)
endfunction()

run_at(o 640 "")
if(NOT "${o}" MATCHES "nav page=collection")
  message(FATAL_ERROR "the Mix row did not open the collection page:\n${o}")
endif()
field_of(of "${o}" "collection kind=playlist [^\n]* of=([0-9]+)" "collection of")
expect_eq("collection songs" "${of}" "2")
field_of(art "${o}" "collection kind=playlist [^\n]* header_art_px=(-?[0-9]+)" "header_art_px")
expect_eq("header art at 640 wide" "${art}" "64")
field_of(flat "${o}" "collection kind=playlist [^\n]* flat=([0-9]+)" "flat")
if(flat LESS 1)
  message(FATAL_ERROR "the collection header was not drawn flat:\n${o}")
endif()

# At 320 wide the sidebar is hidden, so the collection page is entered through --view.
dui_run(o3 --ui desktop --backend mem --view collection --clock fixed --stats --db "${db}" --canvas 320x256 --ydiv 2 --zoom fit --input "${WORK}/idle.txt")
field_of(art3 "${o3}" "colhdr art_px=(-?[0-9]+)" "header_art_px at 320")
expect_eq("header art at 320 wide" "${art3}" "0")

# 4. Backspace back, Shift+Backspace forward.
run_at(b 640 "key backspace\nwait 100\n")
if(NOT "${b}" MATCHES "nav page=home")
  message(FATAL_ERROR "Backspace did not go back to Home:\n${b}")
endif()
run_at(f 640 "key backspace\nwait 100\nkey shift+backspace\nwait 100\n")
if(NOT "${f}" MATCHES "nav page=collection")
  message(FATAL_ERROR "Shift+Backspace did not go forward to the collection page:\n${f}")
endif()
