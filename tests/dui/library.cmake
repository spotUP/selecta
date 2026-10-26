# dui-library (DUI-P4-3 slice A): the Library view with a composer drilldown, and the Stats page (F24). -DSELECTA -DSONGS -DWORK
# 1. --view library lists every composer with its song count (ui_library dump, from the engine's COMPOSERS_SHOW).
# 2. A click on a composer row drills in: the drill line carries the composer's song count from the library (3).
# 3. The Statistics totals in the dump equal `selecta-cli script` @stats_show over the same database.
# 4. A click on the back line leaves the drilldown.
# 5. (slice B) A click on the Tree tab lists the library's songs grouped by format, the nodes with their fixture song
#    counts; a click on a node expands it. A click on the Columns tab lists the same values; a click on a value row drills
#    in and the next column lists that format's songs; the crumb click returns.
# 6. (slice C) The Files tab lists the start directory (the test songs) through the browse source: its folders a, b and c as
#    dir rows; a click on the folder a enters it and lists one.mod and two.mod as file rows. The History tab lists the
#    recently played songs of the seeded history, the newest first.
# The Sources page is not built: the engine has no command that lists the configured sources (reported, not added).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-library needs the sqlite3 command line tool to load the fixture library")
endif()
get_filename_component(BINDIR "${SELECTA}" DIRECTORY)
set(CLI "${BINDIR}/../selecta-cli/selecta-cli")

set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:1', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:2', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:3', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:4', 'jogeir liljedahl', 'Jogeir Liljedahl')")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the composers failed: ${err}")
endif()

function(lib_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --start-dir "${SONGS}" --db "${db}" --ui desktop --backend mem --view library --canvas 900x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

lib_run(list "wait 1\n")
foreach(want "composer 0 chris huelsbeck|Chris Huelsbeck|3" "composer 1 jogeir liljedahl|Jogeir Liljedahl|1")
  string(FIND "${list}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "library list lacks '${want}' in:\n${list}")
  endif()
endforeach()
if("${list}" MATCHES "drill ")
  message(FATAL_ERROR "drilldown shown before any click:\n${list}")
endif()

# I445: the page header breathes. The tab row starts two theme spacings (16 px) below the title row, which ends 36 px
# under the Add Folder button's top (the button sits 4 px into the 40 px row), so neither the title descenders nor the
# buttons touch the tabs; neighbouring tabs keep at least one spacing (8 px) between them, so the active pill never
# touches the next label.
if(NOT "${list}" MATCHES "addfolder (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no addfolder rectangle in the library dump:\n${list}")
endif()
math(EXPR title_end "${CMAKE_MATCH_2} + 36")
string(REGEX MATCHALL "tab [0-9]+ -?[0-9]+ -?[0-9]+ [0-9]+ [0-9]+" tabs "${list}")
set(prev_end "")
foreach(t ${tabs})
  string(REGEX MATCH "tab ([0-9]+) (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)" _ "${t}")
  math(EXPR top_gap "${CMAKE_MATCH_3} - ${title_end}")
  if(top_gap LESS 16)
    message(FATAL_ERROR "tab ${CMAKE_MATCH_1} starts ${top_gap} px under the title row, want at least 16:\n${list}")
  endif()
  if(NOT prev_end STREQUAL "")
    math(EXPR side_gap "${CMAKE_MATCH_2} - ${prev_end}")
    if(side_gap LESS 8)
      message(FATAL_ERROR "tab ${CMAKE_MATCH_1} sits ${side_gap} px after the previous tab, want at least 8:\n${list}")
    endif()
  endif()
  math(EXPR prev_end "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4}")
endforeach()
list(LENGTH tabs ntabs)
if(NOT ntabs EQUAL 5)
  message(FATAL_ERROR "want 5 tab rectangles, got ${ntabs}:\n${list}")
endif()

# The click point comes from the "row 0 x y w h" line the page itself dumps (the rectangle library_click tests).
if(NOT "${list}" MATCHES "row 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row rectangle in the library dump:\n${list}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + 10")
math(EXPR cy "${CMAKE_MATCH_2} + 8")
lib_run(drilled "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 1\n")
if(NOT "${drilled}" MATCHES "drill Chris Huelsbeck\\|3\n")
  message(FATAL_ERROR "click on the first composer row did not drill in to 3 songs:\n${drilled}")
endif()

# Stats totals equal the CLI's.
file(WRITE "${WORK}/stats.sel" "@stats_show\n")
execute_process(COMMAND "${CLI}" script "${WORK}/stats.sel" --db "${db}" OUTPUT_VARIABLE cli RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli failed: ${err}")
endif()
string(REGEX MATCHALL "STAT [^\n]*" cli_rows "${cli}")
list(LENGTH cli_rows n)
if(n EQUAL 0)
  message(FATAL_ERROR "selecta-cli printed no stats:\n${cli}")
endif()
foreach(r IN LISTS cli_rows)
  string(REGEX REPLACE "^STAT ([^=]*)=(.*)$" "stat \\1|\\2" want "${r}")
  string(FIND "${list}" "${want}\n" at)
  if(at LESS 0)
    message(FATAL_ERROR "desktop stats lack '${want}' (from selecta-cli) in:\n${list}")
  endif()
endforeach()

# Back to the list.
# The back line's rectangle comes from the drilldown's own "back x y w h" dump line; it sits above the song rows (I445).
# The second click lands elsewhere than the first so the fixed clock does not fold the two clicks into a double click.
if(NOT "${drilled}" MATCHES "back ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no back rectangle in the drilldown dump:\n${drilled}")
endif()
math(EXPR bx "${CMAKE_MATCH_1} + 60")
math(EXPR by "${CMAKE_MATCH_2} + 8")
lib_run(back "wait 1\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 1\nmove ${bx} ${by}\nclick ${bx} ${by}\nwait 1\n")
if("${back}" MATCHES "drill ")
  message(FATAL_ERROR "back line did not leave the drilldown:\n${back}")
endif()

# ---- Slice B: tree and column browser ----
# The tab rectangles come from the "tab I x y w h" dump lines.
# The fixture songs carry keys like fixture:1 (no file extension), so the engine groups them under (none). Three more songs
# keyed by path give the format groups mod and xm, the keys a scanned library has.
string(CONCAT seed2
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES('/music/a/one.mod', 'local', '/music/a/one.mod', 'One', 'Anon', 'Disk', 'mod', 1990, 'Chiptune', 1000, 1700000010); "
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES('/music/a/two.xm', 'local', '/music/a/two.xm', 'Two', 'Anon', 'Disk', 'xm', 1990, 'Chiptune', 1000, 1700000011); "
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES('/music/b/three.mod', 'local', '/music/b/three.mod', 'Three', 'Anon', 'Disk', 'mod', 1990, 'Chiptune', 1000, 1700000012)")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed2}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the path-keyed songs failed: ${err}")
endif()
foreach(t 1 2)
  if(NOT "${list}" MATCHES "tab ${t} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
    message(FATAL_ERROR "no tab ${t} rectangle in the library dump:\n${list}")
  endif()
  math(EXPR tab${t}x "${CMAKE_MATCH_1} + 10")
  math(EXPR tab${t}y "${CMAKE_MATCH_2} + 8")
endforeach()
string(FIND "${list}" "mode Composers\n" at)
if(at LESS 0)
  message(FATAL_ERROR "the Library page does not start in the composers mode:\n${list}")
endif()

# Six fixture songs (no extension in the key) and three path-keyed songs: (none) 6, mod 2, xm 1, sorted by name.
set(want_groups "(none)|6" "mod|2" "xm|1")
lib_run(tree "wait 2\nmove ${tab1x} ${tab1y}\nclick ${tab1x} ${tab1y}\nwait 2\n")
string(FIND "${tree}" "mode Tree\n" at)
if(at LESS 0)
  message(FATAL_ERROR "a click on the Tree tab did not switch the mode:\n${tree}")
endif()
set(i 0)
foreach(g IN LISTS want_groups)
  string(REPLACE "|" ";" gp "${g}")
  list(GET gp 0 gname)
  list(GET gp 1 gcount)
  string(FIND "${tree}" "vrow ${i} node|0|${gcount}|0|${gname}\n" at)
  if(at LESS 0)
    message(FATAL_ERROR "tree row ${i} is not group ${gname} with ${gcount} songs:\n${tree}")
  endif()
  math(EXPR i "${i} + 1")
endforeach()
if("${tree}" MATCHES "vrow [0-9]+ song")
  message(FATAL_ERROR "the tree shows songs before any node is expanded:\n${tree}")
endif()

# Expand the "mod" node (row 1): its two songs appear below it.
if(NOT "${tree}" MATCHES "vrect 1 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row 1 rectangle in the tree dump:\n${tree}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + 10")
math(EXPR ry "${CMAKE_MATCH_2} + 8")
lib_run(open "wait 2\nmove ${tab1x} ${tab1y}\nclick ${tab1x} ${tab1y}\nwait 2\nmove ${rx} ${ry}\nclick ${rx} ${ry}\nwait 2\n")
string(FIND "${open}" "vrow 1 node|0|2|1|mod\n" at)
if(at LESS 0)
  message(FATAL_ERROR "a click on the mod node did not expand it:\n${open}")
endif()
if(NOT "${open}" MATCHES "vrow 2 song\\|1\\|[0-9]+\\|[0-9]+\\|[^\n]*\nvrect 2 [^\n]*\nvrow 3 song\\|1\\|")
  message(FATAL_ERROR "the expanded mod node does not list its two songs:\n${open}")
endif()

# Column browser: the same values; a click on "mod" drills in and the next column lists its songs.
lib_run(cols "wait 2\nmove ${tab2x} ${tab2y}\nclick ${tab2x} ${tab2y}\nwait 2\n")
string(FIND "${cols}" "mode Columns\n" at)
string(FIND "${cols}" "vrow 1 node|0|2|0|mod\n" at2)
if(at LESS 0 OR at2 LESS 0)
  message(FATAL_ERROR "the Columns tab does not list the format values:\n${cols}")
endif()
if("${cols}" MATCHES "picked ")
  message(FATAL_ERROR "the column browser shows a picked value before any click:\n${cols}")
endif()
lib_run(drill "wait 2\nmove ${tab2x} ${tab2y}\nclick ${tab2x} ${tab2y}\nwait 2\nmove ${rx} ${ry}\nclick ${rx} ${ry}\nwait 2\n")
string(FIND "${drill}" "picked 0|mod\n" at)
if(at LESS 0)
  message(FATAL_ERROR "a click on the mod value did not drill in:\n${drill}")
endif()
# The second column of the format drilldown is the folder: the two mod songs sit in /music/a and /music/b, one each.
foreach(want "vrow 0 node|0|1|0|/music/a\n" "vrow 1 node|0|1|0|/music/b\n")
  string(FIND "${drill}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "the drilled column lacks '${want}':\n${drill}")
  endif()
endforeach()
if("${drill}" MATCHES "vrow 2 ")
  message(FATAL_ERROR "the drilled column lists more than the two mod folders:\n${drill}")
endif()

# The crumb click clears the pick: the format values are back.
if(NOT "${drill}" MATCHES "picked 0\\|mod\n")
  message(FATAL_ERROR "no picked value before the crumb click")
endif()
# The crumb rectangle comes from the stats dump ("crumb x y w h"): the tabs are as wide as their labels (I440).
if(NOT "${drill}" MATCHES "\ncrumb (-?[0-9]+) -?[0-9]+ ([0-9]+) [0-9]+\n" OR CMAKE_MATCH_2 LESS 20)
  message(FATAL_ERROR "no usable crumb rectangle in the stats:\n${drill}")
endif()
math(EXPR cbx "${CMAKE_MATCH_1} + 10")
lib_run(back2 "wait 2\nmove ${tab2x} ${tab2y}\nclick ${tab2x} ${tab2y}\nwait 2\nmove ${rx} ${ry}\nclick ${rx} ${ry}\nwait 2\nmove ${cbx} ${tab2y}\nclick ${cbx} ${tab2y}\nwait 2\n")
if("${back2}" MATCHES "picked ")
  message(FATAL_ERROR "the crumb click did not clear the picked value:\n${back2}")
endif()
string(FIND "${back2}" "vrow 1 node|0|2|0|mod\n" at)
if(at LESS 0)
  message(FATAL_ERROR "the format values are not back after the crumb click:\n${back2}")
endif()

# ---- Slice C: Files and History ----
foreach(t 3 4)
  if(NOT "${list}" MATCHES "tab ${t} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
    message(FATAL_ERROR "no tab ${t} rectangle in the library dump:\n${list}")
  endif()
  math(EXPR tab${t}x "${CMAKE_MATCH_1} + 10")
  math(EXPR tab${t}y "${CMAKE_MATCH_2} + 8")
endforeach()

# Files: the start directory lists the folders a, b and c of the test songs; the parent row comes first.
lib_run(files "wait 2\nmove ${tab3x} ${tab3y}\nclick ${tab3x} ${tab3y}\nwait 2\n")
string(FIND "${files}" "mode Files\n" at)
if(at LESS 0)
  message(FATAL_ERROR "a click on the Files tab did not switch the mode:\n${files}")
endif()
foreach(f a b c)
  if(NOT "${files}" MATCHES "vrow ([0-9]+) dir\\|0\\|0\\|0\\|${f}\n")
    message(FATAL_ERROR "the Files page lacks the folder ${f}:\n${files}")
  endif()
  set(row_${f} ${CMAKE_MATCH_1})
endforeach()
if(NOT "${files}" MATCHES "vrow 0 dir\\|0\\|0\\|0\\|\\.\\.\n")
  message(FATAL_ERROR "the Files page does not start with the parent row:\n${files}")
endif()
if(NOT "${files}" MATCHES "vrect ${row_a} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no rectangle of the folder row a:\n${files}")
endif()
math(EXPR fx "${CMAKE_MATCH_1} + 10")
math(EXPR fy "${CMAKE_MATCH_2} + 8")
lib_run(entered "wait 2\nmove ${tab3x} ${tab3y}\nclick ${tab3x} ${tab3y}\nwait 2\nmove ${fx} ${fy}\nclick ${fx} ${fy}\nwait 2\n")
foreach(f one.mod two.mod)
  if(NOT "${entered}" MATCHES "vrow [0-9]+ file\\|0\\|0\\|0\\|${f}\n")
    message(FATAL_ERROR "a click on the folder a did not list ${f}:\n${entered}")
  endif()
endforeach()
if("${entered}" MATCHES "dir\\|0\\|0\\|0\\|b\n")
  message(FATAL_ERROR "the folder a still lists its sibling folder b:\n${entered}")
endif()

# History: the seeded plays, newest first (Main Theme at 4000, Intro at 2000).
string(CONCAT seed3
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:1', 2000, 0); "
  "INSERT INTO history(key, played_at, skipped) VALUES('fixture:2', 4000, 0)")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed3}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the history failed: ${err}")
endif()
lib_run(hist "wait 2\nmove ${tab4x} ${tab4y}\nclick ${tab4x} ${tab4y}\nwait 2\n")
string(FIND "${hist}" "mode History\n" at)
if(at LESS 0)
  message(FATAL_ERROR "a click on the History tab did not switch the mode:\n${hist}")
endif()
if(NOT "${hist}" MATCHES "vrow [0-9]+ heading\\|[^\n]*Recently played\nvrect [^\n]*\nvrow [0-9]+ play\\|[^\n]*Main Theme[^\n]*\nvrect [^\n]*\nvrow [0-9]+ play\\|[^\n]*Intro")
  message(FATAL_ERROR "the History page does not list Main Theme then Intro under Recently played:\n${hist}")
endif()

# ---- I299: a click on a song row of the composer drilldown plays that song ----
# A scanned song is keyed by its path, so the one song of this composer is keyed by a real module file; OPEN takes that key
# as the path. Before the click nothing is open; after it the engine has opened exactly that one song.
set(db "${WORK}/composer_play.db")
file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/Played.mod")
dui_run(init2 --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
string(CONCAT seed4
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES('${WORK}/Played.mod', 'local', '${WORK}/Played.mod', 'Played', 'Anon', 'Disk', 'mod', 1990, 'Chiptune', 1000, 1700000010); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('${WORK}/Played.mod', 'play composer', 'Play Composer')")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed4}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the playable composer failed: ${err}")
endif()
lib_run(plist "wait 1\n")
if(NOT "${plist}" MATCHES "row 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row 0 rectangle in the playable composer dump:\n${plist}")
endif()
math(EXPR px "${CMAKE_MATCH_1} + 10")
math(EXPR py "${CMAKE_MATCH_2} + 8")
# I445: the first song row of the drilldown is drawn where row 0 of the composer list was (the back line sits above
# the rows, not in row 0); the second click moves sideways so it is not read as a double click
set(sy "${py}")
if(NOT "${plist}" MATCHES "songs_opened=0")
  message(FATAL_ERROR "a song was open before the click:\n${plist}")
endif()
math(EXPR bx "${px} + 60")
lib_run(played "wait 1\nmove ${px} ${py}\nclick ${px} ${py}\nwait 1\nmove ${bx} ${sy}\nclick ${bx} ${sy}\nwait 20\n")
if(NOT "${played}" MATCHES "drill Play Composer\\|1\n")
  message(FATAL_ERROR "the click on the composer did not drill in:\n${played}")
endif()
if(NOT "${played}" MATCHES "songs_opened=1")
  message(FATAL_ERROR "a click on the song row did not open the song:\n${played}")
endif()
# The engine's OPEN only loads; the click is a play intent, so the transport must be playing (play_state 1) from a stopped start.
dui_stat(pstate "${played}" desktop play_state)
if(NOT pstate EQUAL 1)
  message(FATAL_ERROR "a click on the song row loaded the song but did not play it (play_state=${pstate}):\n${played}")
endif()

# ---- I302: a scan finishing inside a composer drilldown keeps the drilldown ----
# Same database: click the composer, then drop a folder (the scan finishes and the Library page reloads). The drilldown must
# still be shown (drill line), and the back line must still return to the composer list.
file(MAKE_DIRECTORY "${WORK}/scandrop")
file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/scandrop/New.mod")
lib_run(scanned "wait 1\nmove ${px} ${py}\nclick ${px} ${py}\nwait 2\ndrop ${WORK}/scandrop\nwait 20\n")
if(NOT "${scanned}" MATCHES "scans 1")
  message(FATAL_ERROR "the dropped folder was not scanned:\n${scanned}")
endif()
if(NOT "${scanned}" MATCHES "drill Play Composer\\|1\n")
  message(FATAL_ERROR "a finished scan moved the user out of the composer drilldown:\n${scanned}")
endif()
# I445: the back line has its own rectangle above the rows; click it where the drilldown dump puts it
if(NOT "${scanned}" MATCHES "\nback ([0-9]+) ([0-9]+) [0-9]+ [0-9]+\n")
  message(FATAL_ERROR "the drilldown after the scan dumped no back line:\n${scanned}")
endif()
math(EXPR sbx "${CMAKE_MATCH_1} + 60")
math(EXPR sby "${CMAKE_MATCH_2} + 8")
lib_run(scanback "wait 1\nmove ${px} ${py}\nclick ${px} ${py}\nwait 2\ndrop ${WORK}/scandrop\nwait 20\nmove ${sbx} ${sby}\nclick ${sbx} ${sby}\nwait 2\n")
if("${scanback}" MATCHES "drill " OR NOT "${scanback}" MATCHES "composer 0 play composer")
  message(FATAL_ERROR "Back after a scan inside the drilldown did not return to the composer list:\n${scanback}")
endif()

# ---- I303: the wheel scrolls the Library composer list (DUI-P3-4, DUI-P4-3) ----
# 200 composers overflow the 600 pixel canvas and the old 4096 byte composer block (about 130 of them). A wheel of 10 notches
# moves the rows up by 160 logical pixels, so row 10 sits where row 0 was minus 320; a click there drills into the eleventh composer (the list starts at 001).
# A wheel far past the end clamps at the content height, which only reaches past 115 rows when the engine holds all 200.
set(db "${WORK}/composer_scroll.db")
dui_run(init3 --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed5
  "INSERT INTO song_composers(song_key, canon, display) WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 200) "
  "SELECT 'fixture:1', printf('scroll composer %03d', i), printf('Scroll Composer %03d', i) FROM n")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed5}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the scroll composers failed: ${err}")
endif()
lib_run(sbase "wait 1\n")
if(NOT "${sbase}" MATCHES "scroll 0\n")
  message(FATAL_ERROR "no scroll 0 line in the 200 composer dump:\n${sbase}")
endif()
if(NOT "${sbase}" MATCHES "row 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row 0 rectangle in the 200 composer dump:\n${sbase}")
endif()
set(sx "${CMAKE_MATCH_1}")
set(sy0 "${CMAKE_MATCH_2}")
set(sh "${CMAKE_MATCH_4}")
math(EXPR sclickx "${sx} + 10")
math(EXPR sy1 "${sy0} + 10 * ${sh} - 160")
math(EXPR sclicky "${sy1} + 8")
lib_run(swheel "wait 1\nwheel 0 -10\nwait 1\n")
if(NOT "${swheel}" MATCHES "scroll 160\n")
  message(FATAL_ERROR "10 wheel notches did not scroll the Library list by 160 pixels:\n${swheel}")
endif()
if(NOT "${swheel}" MATCHES "row 10 ${sx} ${sy1} ")
  message(FATAL_ERROR "row 10 did not move up by 160 pixels to y ${sy1}:\n${swheel}")
endif()
lib_run(sclick "wait 1\nwheel 0 -10\nwait 1\nmove ${sclickx} ${sclicky}\nclick ${sclickx} ${sclicky}\nwait 20\n")
if(NOT "${sclick}" MATCHES "drill Scroll Composer 011\\|")
  message(FATAL_ERROR "a click on the scrolled row 10 did not drill into Scroll Composer 011 (the eleventh):\n${sclick}")
endif()
lib_run(send "wait 1\nwheel 0 -2000\nwait 1\n")
if(NOT "${send}" MATCHES "scroll ([0-9]+)\n")
  message(FATAL_ERROR "no scroll line after the long wheel:\n${send}")
endif()
math(EXPR sfloor "115 * ${sh}")
if(CMAKE_MATCH_1 LESS sfloor)
  message(FATAL_ERROR "the Library list scrolled only ${CMAKE_MATCH_1} pixels: fewer than 115 of the 200 composers reached the page")
endif()

# ---- I310: a wheel notch on Library redraws the main area only, like Songs ----
# One notch used to take the resize path (a full canvas clear and every widget redrawn) on each wheel event, which made the
# scroll lag on a Retina trackpad. The same input with and without the notch must differ by less than the 900x600 canvas
# and draw no extra widgets.
lib_run(snone "wait 1\nmove 450 300\nwait 1\n")
lib_run(snotch "wait 1\nmove 450 300\nwheel 0 -1\nwait 1\n")
if(NOT "${snotch}" MATCHES "scroll 16\n")
  message(FATAL_ERROR "one wheel notch did not scroll the Library list by 16 pixels:\n${snotch}")
endif()
dui_stat(none_total "${snone}" desktop dirty_total)
dui_stat(notch_total "${snotch}" desktop dirty_total)
dui_stat(none_widgets "${snone}" desktop widget_draws)
dui_stat(notch_widgets "${snotch}" desktop widget_draws)
math(EXPR notch_px "${notch_total} - ${none_total}")
math(EXPR canvas_px "900 * 600")
if(notch_px LESS 1 OR NOT notch_px LESS canvas_px)
  message(FATAL_ERROR "one Library wheel notch redrew ${notch_px} pixels; it must redraw more than 0 and less than the ${canvas_px} pixel canvas")
endif()
if(NOT notch_widgets EQUAL none_widgets)
  message(FATAL_ERROR "one Library wheel notch redrew widgets: ${none_widgets} before, ${notch_widgets} after")
endif()

# ---- I481: a composer whose name holds a double quote ----
# The canon folds every non-alphanumeric to a space (sel_composer_canon), so a stored canon never holds a quote: the quote
# lives in the display text only. The Composers page must show the name, and a click on the row must drill in to that
# composer's songs. (The quote escape in desk_composer_request is therefore defensive; no scanned library can reach it.)
set(db "${WORK}/composer_quote.db")
dui_run(init3 --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture for the quoted composer failed: ${err}")
endif()
string(CONCAT seed5
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:1', 'big b smith', 'Big \"B\" Smith'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:2', 'big b smith', 'Big \"B\" Smith')")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed5}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the quoted composer failed: ${err}")
endif()
lib_run(qlist "wait 1\n")
if(NOT "${qlist}" MATCHES "composer 0 big b smith\\|Big \"B\" Smith\\|2\n")
  message(FATAL_ERROR "the Composers page lacks the quoted composer with 2 songs:\n${qlist}")
endif()
if(NOT "${qlist}" MATCHES "row 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row 0 rectangle in the quoted composer dump:\n${qlist}")
endif()
math(EXPR qx "${CMAKE_MATCH_1} + 10")
math(EXPR qy "${CMAKE_MATCH_2} + 8")
lib_run(qdrill "wait 1\nmove ${qx} ${qy}\nclick ${qx} ${qy}\nwait 1\n")
if(NOT "${qdrill}" MATCHES "drill Big \"B\" Smith\\|2\n")
  message(FATAL_ERROR "a click on the quoted composer did not drill in to its 2 songs:\n${qdrill}")
endif()
