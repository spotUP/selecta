# dui-click-map (I678h): every click that opens a page goes through the route table, and Back and Forward replay the history without
# routing (design doc 2026-10-29-desktop-library-architecture.md section 9). -DSELECTA -DSONGS -DWORK
# Entry point: scripted pointer clicks through the real desktop UI (--input), read back from the `route` and `nav` stats lines.
# Sentinel: `route_calls` counts every click handed to the route table (desk_route); a Back or Forward step does not raise it.
# The `route` line: `route route_calls=<n> cur=<page>[:<kind>] n=<history length> pos=<history position> scroll=<collection scroll>
# artist_rect x y w h`.
# Script (the fixture of dui-kind-pages: album 1 "Lotus Turbo Challenge" by Chris Huelsbeck):
#   Home, a click on the sidebar album row (album page), a click on the artist name of its meta line (artist page), Back (album page,
#   same scroll), Back (Home), Forward (album page).
#   cur: home, collection:album, collection:artist, collection:album, home, collection:album; route_calls stays 2 over the last three.
# I678h2: the Home album tile opens the album page by album id (quicktile 0), and Back and Forward onto a collection page restore it
# without COLLECTION_OPEN touching Recents (lib_touch_calls of the `sidelib` line counts the touches).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-click-map needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x1400 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%'; "
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, added) VALUES"
  "('local:fixture/7.mod','local','fixture/7.mod','Seven','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1),"
  "('local:fixture/8.mod','local','fixture/8.mod','Eight','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1); "
  "INSERT INTO history(key, played_at, skipped) VALUES('local:fixture/7.mod', 100, 0)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the click map failed: ${err}")
endif()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()
# The centre of the sidebar row of kind `kind` called `name` into ${out}.
function(side_row out dump kind name)
  if(NOT "${dump}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=${kind} ${name}\n")
    message(FATAL_ERROR "no sidebar row of kind ${kind} '${name}' in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()
# Reads the route line of a dump into ${p}_calls, ${p}_cur, ${p}_n, ${p}_pos, ${p}_scroll and the artist rectangle centre ${p}_artist.
function(read_route p dump)
  if(NOT "${dump}" MATCHES "route route_calls=([0-9]+) cur=([a-z:]+) n=([0-9]+) pos=([0-9]+) scroll=([0-9]+) artist_rect (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
    message(FATAL_ERROR "no route line in:\n${dump}")
  endif()
  set(${p}_calls "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${p}_cur "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${p}_n "${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(${p}_pos "${CMAKE_MATCH_4}" PARENT_SCOPE)
  set(${p}_scroll "${CMAKE_MATCH_5}" PARENT_SCOPE)
  set(${p}_aw "${CMAKE_MATCH_8}" PARENT_SCOPE)
  math(EXPR ax "${CMAKE_MATCH_6} + ${CMAKE_MATCH_8} / 2")
  math(EXPR ay "${CMAKE_MATCH_7} + ${CMAKE_MATCH_9} / 2")
  set(${p}_artist "${ax} ${ay}" PARENT_SCOPE)
endfunction()

set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x1400)
file(WRITE "${WORK}/idle.txt" "wait 100\n")
# Each open touches Recents and reorders the sidebar, so the row is read from an idle run right before the run that clicks it.
function(step out tag script)
  dui_run(idle_${tag} ${common} --input "${WORK}/idle.txt")
  side_row(xy "${idle_${tag}}" 4 "Lotus Turbo Challenge")
  string(REPLACE "ROW" "${xy}" script "${script}")
  file(WRITE "${WORK}/${tag}.txt" "wait 100\n${script}")
  dui_run(run_${tag} ${common} --input "${WORK}/${tag}.txt")
  set(${out} "${run_${tag}}" PARENT_SCOPE)
endfunction()

# 0. Home: nothing routed, the history empty, and where Back and Forward are.
dui_run(home0 ${common} --input "${WORK}/idle.txt")
read_route(r0 "${home0}")
expect_eq("starts on Home" "${r0_cur}" "home")
expect_eq("no click routed yet" "${r0_calls}" "0")
if(NOT "${home0}" MATCHES "nav_arrows (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no nav_arrows line in:\n${home0}")
endif()
math(EXPR back_x "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR back_y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
math(EXPR fwd_x "${CMAKE_MATCH_5} + ${CMAKE_MATCH_7} / 2")
math(EXPR fwd_y "${CMAKE_MATCH_6} + ${CMAKE_MATCH_8} / 2")
set(back "click ${back_x} ${back_y}\nwait 100\n")
set(fwd "click ${fwd_x} ${fwd_y}\nwait 100\n")

# 1. The sidebar album row opens the album page through the route table.
step(d1 s1 "click ROW\nwait 100\n")
read_route(r1 "${d1}")
expect_eq("step 1 page" "${r1_cur}" "collection:album")
expect_eq("step 1 routed" "${r1_calls}" "1")
expect_eq("step 1 history" "${r1_n}/${r1_pos}" "2/1")
if(r1_aw LESS 1)
  message(FATAL_ERROR "the album page draws no clickable artist name:\n${d1}")
endif()

# 2. The artist name on the album page's meta line opens the artist page.
step(d2 s2 "click ROW\nwait 100\nclick ${r1_artist}\nwait 100\n")
read_route(r2 "${d2}")
expect_eq("step 2 page" "${r2_cur}" "collection:artist")
expect_eq("step 2 routed" "${r2_calls}" "2")
expect_eq("step 2 history" "${r2_n}/${r2_pos}" "3/2")

# 3. Back: the album page again, the same scroll, no click routed.
step(d3 s3 "click ROW\nwait 100\nclick ${r1_artist}\nwait 100\n${back}")
read_route(r3 "${d3}")
expect_eq("step 3 page" "${r3_cur}" "collection:album")
expect_eq("step 3 routed" "${r3_calls}" "2")
expect_eq("step 3 history" "${r3_n}/${r3_pos}" "3/1")
expect_eq("step 3 scroll" "${r3_scroll}" "${r1_scroll}")

# 4. Back: Home.
step(d4 s4 "click ROW\nwait 100\nclick ${r1_artist}\nwait 100\n${back}${back}")
read_route(r4 "${d4}")
expect_eq("step 4 page" "${r4_cur}" "home")
expect_eq("step 4 routed" "${r4_calls}" "2")
expect_eq("step 4 history" "${r4_n}/${r4_pos}" "3/0")

# 5. Forward: the album page, replayed without routing.
step(d5 s5 "click ROW\nwait 100\nclick ${r1_artist}\nwait 100\n${back}${back}${fwd}")
read_route(r5 "${d5}")
expect_eq("step 5 page" "${r5_cur}" "collection:album")
expect_eq("step 5 routed" "${r5_calls}" "2")
expect_eq("step 5 history" "${r5_n}/${r5_pos}" "3/1")
expect_eq("step 5 scroll" "${r5_scroll}" "${r1_scroll}")

# 6. D1: the sidebar header title opens nothing, but the route table is asked. This is the third routed click of the run.
if(NOT "${home0}" MATCHES "sidehdr title ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no sidehdr title line in:\n${home0}")
endif()
math(EXPR hx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR hy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
step(d6 s6 "click ROW\nwait 100\nclick ${r1_artist}\nwait 100\n${back}${back}${fwd}click ${hx} ${hy}\nwait 100\n")
read_route(r6 "${d6}")
expect_eq("step 6 page is unchanged by the header title" "${r6_cur}" "collection:album")
expect_eq("step 6 routed (the title counts, opens nothing)" "${r6_calls}" "3")
expect_eq("step 6 history" "${r6_n}/${r6_pos}" "3/1")

# 7. A top bar Home click is routed and lands on Home.
step(d7 s7 "click ROW\nwait 100\ntopbar 0\nwait 100\n")
read_route(r7 "${d7}")
expect_eq("step 7 page" "${r7_cur}" "home")
expect_eq("step 7 routed" "${r7_calls}" "2")

# 8. I678h2: the Home album tile opens the album page by id; the artist link follows; Back, Back, Forward replay without routing and
# without touching Recents (the tile and the artist link are the only two touches).
file(WRITE "${WORK}/tile.txt" "wait 100\nquicktile 0\nwait 100\n")
dui_run(run_tile ${common} --input "${WORK}/tile.txt")
read_route(r8 "${run_tile}")
expect_eq("step 8 page (Home album tile)" "${r8_cur}" "collection:album")
expect_eq("step 8 routed" "${r8_calls}" "1")
expect_eq("step 8 history" "${r8_n}/${r8_pos}" "2/1")
file(WRITE "${WORK}/tile2.txt" "wait 100\nquicktile 0\nwait 100\nclick ${r8_artist}\nwait 100\n${back}${back}${fwd}")
dui_run(run_tile2 ${common} --input "${WORK}/tile2.txt")
read_route(r9 "${run_tile2}")
expect_eq("step 9 page (album, artist, Back, Back, Forward)" "${r9_cur}" "collection:album")
expect_eq("step 9 routed (Forward replay not counted)" "${r9_calls}" "2")
expect_eq("step 9 history" "${r9_n}/${r9_pos}" "3/1")
if(NOT "${run_tile2}" MATCHES "sidelib draws=[0-9]+ rows=[0-9]+ items=[0-9]+ opens=[0-9]+ y=-?[0-9]+ max=[0-9]+ item_calls=[0-9]+ lib_touch_calls=([0-9]+)")
  message(FATAL_ERROR "no sidelib line in:\n${run_tile2}")
endif()
expect_eq("step 9 Recents touched only by the two opens" "${CMAKE_MATCH_1}" "2")

# 10. I678h3: the artist and the album of a track row are links. Open the album page (sidebar row), click the artist link of the first
# row: the artist page; click the album link of the first row: the album page again. Both go through the route table
# (route_calls counts them) and neither plays the song. The `plsonglink` line: `plsonglink <row> artist x y w h album x y w h album_id=<n>`.
function(read_link out_artist out_album dump)
  if(NOT "${dump}" MATCHES "plsonglink 0 artist (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) album (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) album_id=([0-9]+)")
    message(FATAL_ERROR "no plsonglink line in:\n${dump}")
  endif()
  if(CMAKE_MATCH_3 LESS 1 OR CMAKE_MATCH_7 LESS 1 OR CMAKE_MATCH_9 LESS 1)
    message(FATAL_ERROR "the track row draws no artist or album link (widths ${CMAKE_MATCH_3} ${CMAKE_MATCH_7}, album_id ${CMAKE_MATCH_9}):\n${dump}")
  endif()
  math(EXPR ax "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR ay "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  math(EXPR bx "${CMAKE_MATCH_5} + ${CMAKE_MATCH_7} / 2")
  math(EXPR by "${CMAKE_MATCH_6} + ${CMAKE_MATCH_8} / 2")
  set(${out_artist} "${ax} ${ay}" PARENT_SCOPE)
  set(${out_album} "${bx} ${by}" PARENT_SCOPE)
endfunction()
step(d10 s10 "click ROW\nwait 100\n")
read_link(lk_artist lk_album "${d10}")
step(d11 s11 "click ROW\nwait 100\nclick ${lk_artist}\nwait 100\n")
read_route(r11 "${d11}")
expect_eq("step 11 page (track-row artist link)" "${r11_cur}" "collection:artist")
expect_eq("step 11 routed" "${r11_calls}" "2")
step(d12 s12 "click ROW\nwait 100\nclick ${lk_album}\nwait 100\n")
read_route(r12 "${d12}")
expect_eq("step 12 page (track-row album link)" "${r12_cur}" "collection:album")
expect_eq("step 12 routed" "${r12_calls}" "2")
expect_eq("step 12 history (a new entry: the link carries its own id and key)" "${r12_n}/${r12_pos}" "3/2")

# 13. I678h3: the composer on the Song Information page is a link (SEL_ROUTE_SONGINFO_COMPOSER) to the composer page. The song row names
# its composer in song_composers; the Author row of the open module shows it. The `sicomposer x y w h key=<canon>` line gives the rect.
configure_file("${SONGS}/selecta-test.mod" "${WORK}/one.mod" COPYONLY)
set(sidb "${WORK}/si.db")
dui_run(sinit --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${sidb}")
string(CONCAT siseed
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, added) VALUES"
  "('local:${WORK}/one.mod','local','${WORK}/one.mod','Test','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1); "
  "INSERT INTO song_composers(song_key, canon, display, tag) VALUES('local:${WORK}/one.mod','chris huelsbeck','Chris Huelsbeck',1)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${sidb}" "${siseed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the Song Information composer failed: ${err}")
endif()
set(sicommon --ui desktop --backend mem --view songinfo --canvas 1280x800 --clock fixed --stats --db "${sidb}" --exec "OPEN ${WORK}/one.mod")
file(WRITE "${WORK}/si_idle.txt" "wait 100\n")
dui_run(si_idle ${sicommon} --input "${WORK}/si_idle.txt")
if(NOT "${si_idle}" MATCHES "sicomposer (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) key=chris huelsbeck")
  message(FATAL_ERROR "no sicomposer line in:\n${si_idle}")
endif()
math(EXPR sx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR sy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
file(WRITE "${WORK}/si_click.txt" "wait 100\nclick ${sx} ${sy}\nwait 100\n")
dui_run(si_click ${sicommon} --input "${WORK}/si_click.txt")
read_route(r13 "${si_click}")
expect_eq("step 13 page (Song Information composer link)" "${r13_cur}" "collection:composer")
expect_eq("step 13 routed" "${r13_calls}" "1")

# 14. I690b: a link of a track row has a hover look. With the pointer on the artist link the `plhover` line reports link=1 (the link is
# underlined), on the album link link=2, and the row under the pointer is filled (fill is not 0: the link must not take the row's
# hover away). Off the links, link=0.
step(d14 s14 "click ROW\nwait 100\nmove ${lk_artist}\nwait 100\n")
if(NOT "${d14}" MATCHES "plhover fill=([0-9a-f]+) link=([0-9]+)")
  message(FATAL_ERROR "no plhover line in:\n${d14}")
endif()
expect_eq("step 14 artist link hover" "${CMAKE_MATCH_2}" "1")
if(CMAKE_MATCH_1 STREQUAL "00000000")
  message(FATAL_ERROR "step 14: the row under the artist link has no hover fill")
endif()
step(d15 s15 "click ROW\nwait 100\nmove ${lk_album}\nwait 100\n")
string(REGEX MATCH "plhover fill=([0-9a-f]+) link=([0-9]+)" d15m "${d15}")
expect_eq("step 15 album link hover" "${CMAKE_MATCH_2}" "2")
step(d16 s16 "click ROW\nwait 100\nmove 5 5\nwait 100\n")
string(REGEX MATCH "plhover fill=([0-9a-f]+) link=([0-9]+)" d16m "${d16}")
expect_eq("step 16 no link hover off the links" "${CMAKE_MATCH_2}" "0")
