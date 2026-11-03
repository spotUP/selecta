# dui-nav-turn-cost (I959): a navigation turn of the live desktop UI stays under 50 ms on a large library. -DSELECTA -DWORK -DSDL_BUILT
# Report (I906h): "Sources chip turn_ms_max 397 ms, opening the Modland page 233 ms" on a real library copy. Measured in I959: those numbers are
# the scripted --input replay, which runs a whole script (its waits included) inside ONE desk_wait turn; the live SDL loop, where a person
# navigates, peaks at 25 ms (Sources chip then the Modland sidebar row) on the 780 MB real library. The real defect found by the live loop:
# a Browse format tile (a search with a big result) took 62 s of pump time at 200000 songs, 19 s at 100000: search_reload, search_pick_songs
# and the QUERY view walked the result with sel_engine_result_key(i), which rescans from the start, O(n^2).
# This test drives the real SDL loop with timed mouse events on a 50000 song Modland fixture (the old walk took seconds here; the linear
# search_reload work costs about 0.35 ms per 1000 songs, 73 ms at 200000: open item in agent-reports/I959.md):
# 1. Sources chip (five right-chevron steps), then the Modland sidebar row; 2. topbar Browse, then the Impulsetracker format tile;
# 3. topbar Browse, then the Modland source tile.
# Per run: turn_ms_max < 50 and pump_ms_max < 50. Sentinels: the chip is in force and the page really opened, so a missed click cannot pass.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-nav-turn-cost needs the sqlite3 command line tool to seed the library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x1000 --clock fixed --stats --db "${db}")
string(CONCAT seed
  "DROP TRIGGER IF EXISTS songs_ai; "
  "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 50000) "
  "INSERT INTO songs(key, source, path, name, artist, album, format, added) "
  "SELECT 'modland:' || i, 'modland', '/m/' || i || '.mod', 'Song ' || i, 'Artist ' || (i % 2000), '', "
  "CASE i % 3 WHEN 0 THEN 'Protracker' WHEN 1 THEN 'Fasttracker 2' ELSE 'Impulsetracker' END, 1 FROM n")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the large library failed: ${err}")
endif()
file(COPY_FILE "${db}" "${WORK}/base.db")

function(click out x y t)
  math(EXPR t1 "${t} + 50")
  math(EXPR t2 "${t} + 100")
  set(${out} "m${x},${y}@${t} d1,${x},${y}@${t1} u1,${x},${y}@${t2}" PARENT_SCOPE)
endfunction()

function(live_run outvar events)
  file(COPY_FILE "${WORK}/base.db" "${db}" ONLY_IF_DIFFERENT)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(MAKE_DIRECTORY "${WORK}/cfg")
  file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
  set(ENV{SELECTA_SDL_TEST_LIVE} 1)
  set(ENV{SELECTA_SOCKET} "/tmp/selecta-dui-nav-turn-cost.sock")
  set(ENV{SELECTA_SDL_TEST_TIMED} "${events}")
  execute_process(COMMAND "${SELECTA}" --config-dir "${WORK}/cfg" --db "${db}" --ui desktop --backend sdl --view home --canvas 1280x1000
    --clock fixed --stats OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${e}")
  endif()
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# One scenario's verdict. A turn is a wall-clock number, so a loaded machine (emulators, a compiler) can push a healthy turn over 50 ms once;
# a real regression is seconds in every run. The run is repeated up to 3 times and the best turn_ms_max / pump_ms_max decide; the sentinel
# list (semicolon separated regexes) must hold in every run.
function(check_turns label events sentinel)
  set(best_turn 999999)
  set(best_pump 999999)
  foreach(attempt RANGE 1 3)
    live_run(o "${events}")
    if(NOT "${o}" MATCHES "turn_ms_max=([0-9]+)")
      message(FATAL_ERROR "${label}: no turn_ms_max in:\n${o}")
    endif()
    set(turn ${CMAKE_MATCH_1})
    if(NOT "${o}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+)")
      message(FATAL_ERROR "${label}: no ui_stats_pump line in:\n${o}")
    endif()
    set(pump ${CMAKE_MATCH_1})
    foreach(sent IN LISTS sentinel)
      if(NOT "${o}" MATCHES "${sent}")
        message(FATAL_ERROR "${label}: the page never opened (sentinel ${sent}):\n${o}")
      endif()
    endforeach()
    if(turn LESS best_turn)
      set(best_turn ${turn})
    endif()
    if(pump LESS best_pump)
      set(best_pump ${pump})
    endif()
    if(best_turn LESS 50 AND best_pump LESS 50)
      break()
    endif()
  endforeach()
  if(NOT best_turn LESS 50)
    message(FATAL_ERROR "${label}: best turn_ms_max of 3 runs is ${best_turn}, want < 50")
  endif()
  if(NOT best_pump LESS 50)
    message(FATAL_ERROR "${label}: best pump_ms_max of 3 runs is ${best_pump}, want < 50")
  endif()
  message(STATUS "dui-nav-turn-cost ${label} ok (turn_ms_max=${best_turn} pump_ms_max=${best_pump})")
endfunction()

# 1. Sources chip (five right-chevron steps), then the Modland sidebar row. Both targets are read from idle dumps of the same
# library at the live canvas (the sidebar moves with every layout change; I1010b): the right chevron from a plain idle run, the
# Modland row (kind source) from a run that has stepped the chip five times, since the chip filters the list.
set(common_idle --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x1000)
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(idle0 ${common_idle} --input "${WORK}/idle.txt")
if(NOT "${idle0}" MATCHES "sidechev L [0-9]+ [0-9]+ [0-9]+ [0-9]+ R ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no sidechev line in:\n${idle0}")
endif()
math(EXPR chip_x "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR chip_y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
# The top bar Browse button, read from the same dump (it sits right of the search field; a fixed pixel there hit the field).
if(NOT "${idle0}" MATCHES "topbar draws=[0-9]+ shown=1 home [-0-9]+ [-0-9]+ [-0-9]+ [-0-9]+ field [-0-9]+ [-0-9]+ [-0-9]+ [-0-9]+ browse ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no topbar line in:\n${idle0}")
endif()
math(EXPR browse_x "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR browse_y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(chip_steps "")
foreach(k RANGE 1 5)
  string(APPEND chip_steps "click ${chip_x} ${chip_y}\nwait 100\n")
endforeach()
file(WRITE "${WORK}/chip5.txt" "${chip_steps}")
dui_run(idle5 ${common_idle} --input "${WORK}/chip5.txt")
# The centre of the sidebar row of kind `kind` called `name` (copy of side_row in kind_pages.cmake, 0fadc05b).
if(NOT "${idle5}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=7 Modland\n")
  message(FATAL_ERROR "no Modland sidebar row after the Sources chip in:\n${idle5}")
endif()
math(EXPR row_x "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR row_y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(ev "")
set(t 4000)
foreach(k RANGE 1 5)
  click(c ${chip_x} ${chip_y} ${t})
  string(APPEND ev "${c} ")
  math(EXPR t "${t} + 1500")
endforeach()
click(c ${row_x} ${row_y} ${t})
math(EXPR tq "${t} + 4000")
check_turns("sources-chip-then-row" "${ev}${c} q@${tq}" "sidechips chip=5;collection kind=source opens=1 ")

# 2. Topbar Browse, then the Impulsetracker format tile (browserect 481 260 217 88): a search whose result list search_reload walks.
click(b ${browse_x} ${browse_y} 4000)
click(m 589 304 8000)
check_turns("browse-then-format-tile" "${b} ${m} q@15000" "search draws [0-9]+ rows [0-9]+ query format:impulsetracker")

# 3. Topbar Browse, then the Modland source tile (browserect 481 548 217 88): the source page, no search.
click(m 589 592 8000)
check_turns("browse-then-source-tile" "${b} ${m} q@12000" "collection kind=source opens=1 ")
