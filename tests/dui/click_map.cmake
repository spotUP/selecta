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
# Gap, not tested: a Home album tile carries the album name, not the album id, so it cannot open the album page yet (report I678h).
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
  "('local:fixture/8.mod','local','fixture/8.mod','Eight','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1)")
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
