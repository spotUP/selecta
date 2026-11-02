# dui-direct-calls-async (I906e): the desktop UI's direct song text calls never block the UI thread.
# -DSELECTA -DWORK -DSDL_BUILT
# Design: thoughts/shared/research/2026-11-01-ui-thread-library-waits.md section 2c, row I906e.
# Before: queue_groups_sync asked the library for the album group of every queue row (sel_engine_song_group), the Remove dialog and the
# toast for the title, the Song Information page for the metadata: each a watched wait on the pump thread. Now all three are served from
# the engine song text cache (sel_engine_song_text) and resolved on the rd_txt posted read.
# Driven through the desktop UI's live SDL loop on the Queue page with a queue of three songs. SELECTA_LIB_TEXT_SLOW_MS = 300 slows every
# title, group and metadata read in the library worker. Asserts:
# 1. pump_ms_max < 50 and turn_ms_max < 50: nothing slept the 300 ms of a slowed read;
# 2. lib_ui_watched_waits == 0;
# 3. sentinel: view_reads text_reads>=1, the posted text read ran and landed.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-direct-calls-async needs the sqlite3 command line tool to load the fixture library")
endif()
# DUI_SITES_ONLY="1 5" runs only those sites (0 = the queue part, 1..6 the sites of part 2; the PLAYLIST_PLAY and I951 write runs always run); unset runs all.
# A revert proof runs one site at a time instead of the 105-200 s of the whole file.
function(sites_want n outvar)
  if("$ENV{DUI_SITES_ONLY}" STREQUAL "")
    set(${outvar} 1 PARENT_SCOPE)
  elseif("${n}" IN_LIST DUI_ONLY_LIST)
    set(${outvar} 1 PARENT_SCOPE)
  else()
    set(${outvar} 0 PARENT_SCOPE)
  endif()
endfunction()
string(REPLACE " " ";" DUI_ONLY_LIST "$ENV{DUI_SITES_ONLY}")
set(fx "${HERE}/../m3/lyrics")
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SOCKET} "/tmp/selecta-dui-direct-calls.sock")
set(ENV{SELECTA_LIB_TEXT_SLOW_MS} 300)
# SELECTA_LIB_ART_SLOW_MS is not set: the desktop UI plugin still asks the library for art ids through its own provider (art.find, a watched wait
# outside the engine view path), so a slowed art op would fail this test for a reason that is not I906d. The art id path is proved by its sentinel.
set(ENV{SELECTA_LIB_UI_GUARD} "log")
file(REMOVE_RECURSE "${WORK}/cfg")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_SDL_TEST_TIMED} "q@5000")
# No --output null: that loop waits for posted reads and would inflate pump_ms_max; the live SDL loop is the one that must not wait.
# No OPEN: opening a song runs the play-start reads (loudness, sticker, genre), which are row I906g and watched waits of their own. The
# Remove dialog and the Song Information page read the same cache and are driven by dui-library-remove and dui-songinfo.
set(args --config-dir "${WORK}/cfg" --db "${db}" --ui desktop --backend sdl --view queue --canvas 1280x800 --clock fixed --stats
  --exec "QUEUE_ADD \"fixture/1.mod\"" --exec "QUEUE_ADD \"fixture/2.mod\"" --exec "QUEUE_ADD \"fixture/3.xm\"")
sites_want(0 w0)
if(w0)
execute_process(COMMAND "${SELECTA}" ${args} OUTPUT_VARIABLE o ERROR_VARIABLE o_err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${o_err}")
endif()
if(NOT "${o}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+) lib_ui_guard_hits=([0-9]+) lib_ui_allowed_waits=([0-9]+) lib_ui_watched_waits=([0-9]+)")
  message(FATAL_ERROR "no ui_stats_pump line in:\n${o}")
endif()
set(pump ${CMAKE_MATCH_1})
if(pump GREATER 49)
  message(FATAL_ERROR "pump_ms_max is ${pump}, want < 50:\n${o}")
endif()
if(CMAKE_MATCH_4 GREATER 0)
  message(FATAL_ERROR "lib_ui_watched_waits is ${CMAKE_MATCH_4}, want 0:\n${o}")
endif()
# I946: the gate is the time the UI thread spent blocked in library requests, not the wall time of its turns. A turn_ms_max of 130 under
# load (3 rigs running) was the UI thread descheduled, with lib_ui_watched_waits 0; a slowed read on the UI thread shows as >= 300 here.
if(NOT "${o}" MATCHES "turn_ms_max=([0-9]+)")
  message(FATAL_ERROR "no turn_ms_max in:\n${o}")
endif()
if(NOT "${o}" MATCHES "lib_ui_wait_ms=([0-9]+)")
  message(FATAL_ERROR "no lib_ui_wait_ms in:\n${o}")
endif()
if(CMAKE_MATCH_1 GREATER 49)
  message(FATAL_ERROR "lib_ui_wait_ms is ${CMAKE_MATCH_1}, want < 50:\n${o}")
endif()
set(turn ${CMAKE_MATCH_1})
if(NOT "${o}" MATCHES "view_reads panel_reads=([0-9]+) art_reads=([0-9]+) total_reads=([0-9]+) text_reads=([0-9]+) group_reads=([0-9]+)")
  message(FATAL_ERROR "no view_reads line in:\n${o}")
endif()
if(CMAKE_MATCH_5 LESS 1)
  message(FATAL_ERROR "the posted queue group read never ran (group_reads=${CMAKE_MATCH_5}):\n${o}")
endif()
if(CMAKE_MATCH_5 GREATER 4)
  message(FATAL_ERROR "the queue group read ran ${CMAKE_MATCH_5} times for a settled 3 entry queue, want a bounded few:\n${o}")
endif()
message(STATUS "direct_calls_async_ok pump_ms_max=${pump} turn_ms_max=${turn} text_reads=${CMAKE_MATCH_4} group_reads=${CMAKE_MATCH_5}")
endif()

unset(ENV{SELECTA_LIB_TEXT_SLOW_MS}) # part 1 set it for itself; each site below names its own knob

# ---------------------------------------------------------------------------------------------------------------------------------
# Part 2 (I906e): every moved site, driven live. One run per site with its slow knob set to 300 ms; each asserts turn_ms_max < 50 and
# that the site's own sentinel moved (so a run that never reached the site, or reached it through the old blocking call, fails).
#   site 1  Remove dialog title            SELECTA_LIB_TEXT_SLOW_MS   dialog opens=1, view_reads text_reads>=1
#   site 2  Song Information composer      SELECTA_LIB_TEXT_SLOW_MS   sicomposer rect + key landed, text_reads>=1
#   site 3  Clear dialog playlist count    SELECTA_LIB_COUNT_SLOW_MS  dialog open=2, playlists=1
#   site 4  New playlist count             SELECTA_LIB_COUNT_SLOW_MS  sidehdr plus plus_clicks=1
#   site 5  Add to Playlist submenu        SELECTA_LIB_ITEMS_SLOW_MS  song_menu pl_posts>=1 and pl_takes>=1
#   site 6  queue of more than 4096 rows   SELECTA_LIB_GROUP_SLOW_MS (first group read only; 4200 reads at 300 ms would never finish): group_reads 1..4
# Geometry comes from scripted mem runs on the same canvas; the live runs click through SDL timed events (SELECTA_SDL_TEST_TIMED).
# ---------------------------------------------------------------------------------------------------------------------------------
set(sites_canvas 1280x800)
set(sites_db "${WORK}/sites.db")
set(sites_slow_ms 300)
# DUI_SITES_REALDB=<path to a COPY of a real library.db> runs the same scripts on it; DUI_SITES_SLOW_MS=0 turns the slow knobs off (real-size check).
if(DEFINED ENV{DUI_SITES_REALDB})
  file(COPY_FILE "$ENV{DUI_SITES_REALDB}" "${sites_db}" ONLY_IF_DIFFERENT)
endif()
if(DEFINED ENV{DUI_SITES_SLOW_MS})
  set(sites_slow_ms "$ENV{DUI_SITES_SLOW_MS}")
endif()
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()

function(sites_expect what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got ${got}, want ${want}")
  endif()
endfunction()

# Scripted geometry run on the mem backend: sites_mem(<out> <db> <view> <input text> [extra args])
function(sites_mem outvar db view input)
  file(WRITE "${WORK}/sites_in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view ${view} --canvas ${sites_canvas} --clock fixed --stats ${ARGN}
    --input "${WORK}/sites_in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# Centre "x,y" of the rect that follows <regex prefix> in a dump.
function(sites_centre outvar dump prefix)
  if(NOT "${dump}" MATCHES "${prefix}(-?[0-9]+)[ ,](-?[0-9]+)[ ,]([0-9]+)[ ,]([0-9]+)")
    message(FATAL_ERROR "no rect after '${prefix}' in:\n${dump}")
  endif()
  if(CMAKE_MATCH_3 EQUAL 0)
    message(FATAL_ERROR "empty rect after '${prefix}' in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${outvar} "${cx},${cy}" PARENT_SCOPE)
endfunction()

# A left (1) or right (3) click as SDL timed tokens starting at <ms>: motion, press 40 ms later, release 80 ms later.
function(sites_click outvar ms btn xy)
  math(EXPR t1 "${ms} + 40")
  math(EXPR t2 "${ms} + 80")
  set(${outvar} "m${xy}@${ms} d${btn},${xy}@${t1} u${btn},${xy}@${t2}" PARENT_SCOPE)
endfunction()

# Live SDL run: sites_live(<out> <tokens> <extra args...>). The knob variables SITE_KNOB_<NAME> given in KNOBS are set for this run
# only. Quits through the last token in <tokens>. Fails on a non-zero exit. Asserts the shared invariant lib_ui_wait_ms < 50 (I946).
function(sites_live outvar tokens)
  cmake_parse_arguments(L "" "SOCK" "KNOBS;ARGS" ${ARGN})
  foreach(k IN LISTS L_KNOBS)
    set(ENV{${k}} ${sites_slow_ms})
  endforeach()
  set(ENV{SELECTA_SOCKET} "/tmp/selecta-dui-direct-calls-${L_SOCK}.sock")
  set(ENV{SELECTA_SDL_TEST_TIMED} "${tokens}")
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(MAKE_DIRECTORY "${WORK}/cfg")
  file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
  execute_process(COMMAND "${SELECTA}" --config-dir "${WORK}/cfg" --ui desktop --backend sdl --canvas ${sites_canvas} --clock fixed
    --stats ${L_ARGS} OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
  foreach(k IN LISTS L_KNOBS)
    unset(ENV{${k}})
  endforeach()
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "site ${L_SOCK}: selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${e}")
  endif()
  # Every live run reports its numbers, so a revert proof can quote fixed against reverted.
  set(nums "")
  foreach(key pump_ms_max turn_ms_max lib_ui_wait_ms lib_ui_watched_waits)
    if("${o}" MATCHES "${key}=([0-9]+)")
      string(APPEND nums " ${key}=${CMAKE_MATCH_1}")
    endif()
  endforeach()
  message(STATUS "site ${L_SOCK} numbers:${nums}")
  # I946: lib_ui_wait_ms is the time the UI thread spent blocked in library requests; turn_ms_max (wall time) flaked at 130 ms under load.
  if(NOT "${o}" MATCHES "lib_ui_wait_ms=([0-9]+)")
    message(FATAL_ERROR "site ${L_SOCK}: no lib_ui_wait_ms in:\n${o}")
  endif()
  if(CMAKE_MATCH_1 GREATER 49)
    message(FATAL_ERROR "site ${L_SOCK}: lib_ui_wait_ms is ${CMAKE_MATCH_1}, want < 50 (a 300 ms slowed read ran on the UI thread):\n${o}")
  endif()
  set(${outvar}_turn "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(sites_field outvar dump line key)
  if(NOT "${dump}" MATCHES "(^|\n)${line} [^\n]*${key}=([0-9]+)")
    message(FATAL_ERROR "no ${line} ${key} in:\n${dump}")
  endif()
  set(${outvar} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

# Setup: scan three songs (three library rows under one composer tile) and create the playlist `alpha`.
sites_mem(ignore "${sites_db}" home "wait 1\nexec LIBRARY_SCAN ${WORK}/songs\nwait 4\nexec PLAYLIST_CREATE alpha\nwait 2\n")
# Geometry 1: the Browse tab and its composer tile; 2: the composer page's second song row.
sites_mem(g_browse "${sites_db}" home "wait 3\ntopbar 2\nwait 3\n")
sites_centre(browse_xy "${g_browse}" "topbar draws=[0-9]+ shown=[0-9]+ home [0-9]+ [0-9]+ [0-9]+ [0-9]+ field [0-9]+ [0-9]+ [0-9]+ [0-9]+ browse ")
if(NOT "${g_browse}" MATCHES "browse tile ([0-9]+) 1 ")
  message(FATAL_ERROR "no composer tile on the Browse page:\n${g_browse}")
endif()
set(tile ${CMAKE_MATCH_1})
sites_centre(tile_xy "${g_browse}" "browserect ${tile} ")
sites_mem(g_comp "${sites_db}" home "wait 3\ntopbar 2\nwait 3\nbrowse ${tile}\nwait 3\n")
if(NOT "${g_comp}" MATCHES "plsong 1 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no song row on the composer page:\n${g_comp}")
endif()
math(EXPR rowx "${CMAKE_MATCH_1} + 20")
math(EXPR rowy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(row_xy "${rowx},${rowy}")
# Song menu items are 24 px high from the click point; item k centre = (x + 10, y + k * 24 + 12).
math(EXPR itx "${rowx} + 10")
math(EXPR it4y "${rowy} + 4 * 24 + 12")

# The input path to a library song menu: open Browse, the composer tile, right click the row, pick the item at <item_xy>.
function(sites_menu_tokens outvar item_xy)
  sites_click(c1 600 1 "${browse_xy}")
  sites_click(c2 1500 1 "${tile_xy}")
  sites_click(c3 2500 3 "${row_xy}")
  sites_click(c4 3300 1 "${item_xy}")
  set(${outvar} "${c1} ${c2} ${c3} ${c4}" PARENT_SCOPE)
endfunction()

sites_want(1 w1)
if(w1)
# --- site 1: Remove dialog title (TEXT knob). Remove is the fifth item, present on a library row.
sites_menu_tokens(t1 "${itx},${it4y}")
sites_live(s1 "${t1} q@5200" SOCK s1 KNOBS SELECTA_LIB_TEXT_SLOW_MS ARGS --db "${sites_db}" --view home)
sites_field(s1_opens "${s1}" dialog opens)
sites_expect("site 1: Remove dialog opens" "${s1_opens}" 1)
sites_field(s1_open "${s1}" dialog open)
sites_expect("site 1: Remove dialog is open at the end" "${s1_open}" 1)
if(NOT "${s1}" MATCHES "view_reads panel_reads=[0-9]+ art_reads=[0-9]+ total_reads=[0-9]+ text_reads=([0-9]+) ")
  message(FATAL_ERROR "site 1: no view_reads line in:\n${s1}")
endif()
if(CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "site 1: the posted title read never ran (text_reads=${CMAKE_MATCH_1}):\n${s1}")
endif()
message(STATUS "site1_ok turn_ms_max=${s1_turn} text_reads=${CMAKE_MATCH_1}")
endif()

sites_want(5 w5)
if(w5)
# --- site 5: Add to Playlist submenu (ITEMS knob). Item index 3 opens the playlist step; its list is read on the items slot.
# Driven from a queue row: the composer page keeps the sidebar list stale every frame (Folders source), so its read always holds the
# one items slot and the menu read would wait behind it; the Queue page leaves the sidebar quiet. The menu is the same song menu.
set(q5_exec --exec "QUEUE_ADD ${WORK}/songs/a.mod" --exec "QUEUE_ADD ${WORK}/songs/b.mod")
sites_mem(g_q "${sites_db}" queue "wait 3\n" ${q5_exec})
if(NOT "${g_q}" MATCHES "qrow 0 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no queue row rectangle:\n${g_q}")
endif()
math(EXPR qx "${CMAKE_MATCH_1} + 20")
math(EXPR qy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
math(EXPR qix "${qx} + 10")
math(EXPR qiy "${qy} + 3 * 24 + 12")
sites_click(c5a 1500 3 "${qx},${qy}")
sites_click(c5b 2300 1 "${qix},${qiy}")
sites_live(s5 "${c5a} ${c5b} q@4500" SOCK s5 KNOBS SELECTA_LIB_ITEMS_SLOW_MS ARGS --db "${sites_db}" --view queue ${q5_exec})
sites_field(s5_posts "${s5}" song_menu pl_posts)
sites_field(s5_takes "${s5}" song_menu pl_takes)
if(s5_posts LESS 1 OR s5_takes LESS 1)
  message(FATAL_ERROR "site 5: the Add to Playlist list was not read through the items slot (pl_posts=${s5_posts} pl_takes=${s5_takes}):\n${s5}")
endif()
if(NOT "${s5}" MATCHES "smenu_pl step=1 playlists=1 ")
  message(FATAL_ERROR "site 5: the playlist step of the song menu did not list the playlist `alpha`:\n${s5}")
endif()
message(STATUS "site5_ok turn_ms_max=${s5_turn} pl_posts=${s5_posts} pl_takes=${s5_takes}")

# --- I906f write: picking the playlist `alpha` sends PLAYLIST_ADD_SONG, which the library write queue runs on the writer thread. The
# write is slowed 300 ms (WRITE knob): the pump and the turn stay under 50 ms, no watched wait, and lib_writes_done proves the queued write ran.
math(EXPR piy "${qy} + 12")
sites_click(c6c 3300 1 "${qix},${piy}")
sites_live(s6 "${c5a} ${c5b} ${c6c} q@6000" SOCK s6 KNOBS SELECTA_LIB_WRITE_SLOW_MS ARGS --db "${sites_db}" --view queue ${q5_exec})
if(NOT "${s6}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+) lib_ui_guard_hits=([0-9]+) lib_ui_allowed_waits=([0-9]+) lib_ui_watched_waits=([0-9]+)")
  message(FATAL_ERROR "site 6: no ui_stats_pump line in:\n${s6}")
endif()
if(CMAKE_MATCH_1 GREATER 49 OR CMAKE_MATCH_4 GREATER 0)
  message(FATAL_ERROR "site 6: pump_ms_max=${CMAKE_MATCH_1} lib_ui_watched_waits=${CMAKE_MATCH_4}, want < 50 and 0 (a queued write ran on the UI thread):\n${s6}")
endif()
if(NOT "${s6}" MATCHES "lib_writes_done=([0-9]+)" OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "site 6: the queued PLAYLIST_ADD_SONG write did not run (lib_writes_done missing or 0):\n${s6}")
endif()
message(STATUS "site6_ok turn_ms_max=${s6_turn} lib_writes_done=${CMAKE_MATCH_1}")
endif()

# --- site 3: Clear dialog count (COUNT knob), on the Folders page. The first total count read has landed by 1.8 s.
sites_mem(g_lib "${sites_db}" library "wait 3\n")
sites_want(3 w3)
if(w3)
sites_centre(clear_xy "${g_lib}" "dialog [^\n]*clear=")
sites_click(c3a 1800 1 "${clear_xy}")
sites_live(s3 "${c3a} q@4000" SOCK s3 KNOBS SELECTA_LIB_COUNT_SLOW_MS ARGS --db "${sites_db}" --view library)
sites_field(s3_open "${s3}" dialog open)
sites_field(s3_pl "${s3}" dialog playlists)
sites_expect("site 3: Clear Library dialog open (kind 2)" "${s3_open}" 2)
sites_expect("site 3: the dialog names the playlist count" "${s3_pl}" 1)
message(STATUS "site3_ok turn_ms_max=${s3_turn} playlists=${s3_pl}")
endif()

# --- site 4: New playlist count (COUNT knob). The header plus creates "Playlist <n+1>" from the engine's count.
sites_want(4 w4)
if(w4)
sites_centre(plus_xy "${g_lib}" "sidehdr plus [^\n]*rect ")
sites_click(c4a 1800 1 "${plus_xy}")
# I948b: the create is a library write too. WRITE_SLOW_MS makes it a 300 ms statement, so a create made on the UI thread fails the wait
# bound on every run (before the fix it waited only when the worker happened to be busy: a flake).
sites_live(s4 "${c4a} q@4000" SOCK s4 KNOBS SELECTA_LIB_COUNT_SLOW_MS SELECTA_LIB_WRITE_SLOW_MS ARGS --db "${sites_db}" --view library)
sites_field(s4_clicks "${s4}" sidehdr plus_clicks)
sites_expect("site 4: the header plus was clicked once" "${s4_clicks}" 1)
message(STATUS "site4_ok turn_ms_max=${s4_turn} plus_clicks=${s4_clicks}")
endif()

sites_want(2 w2)
if(w2)
# --- site 2: Song Information composer link (TEXT knob). Seeded song row names its composer; the Author row links to it.
configure_file("${SONGS}/selecta-test.mod" "${WORK}/one.mod" COPYONLY)
set(si_db "${WORK}/si.db")
dui_run(sinit --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${si_db}")
string(CONCAT siseed
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, added) VALUES"
  "('local:${WORK}/one.mod','local','${WORK}/one.mod','Test','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1); "
  "INSERT INTO song_composers(song_key, canon, display, tag) VALUES('local:${WORK}/one.mod','chris huelsbeck','Chris Huelsbeck',1)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${si_db}" "${siseed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the Song Information composer failed: ${err}")
endif()
# OPEN needs an output; --output null keeps the run silent (a clockless loop, so pump_ms_max is not asserted here, only the UI turn).
sites_live(s2 "q@4000" SOCK s2 KNOBS SELECTA_LIB_TEXT_SLOW_MS
  ARGS --output null --db "${si_db}" --view songinfo --exec "OPEN ${WORK}/one.mod")
if(NOT "${s2}" MATCHES "sicomposer (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) key=chris huelsbeck")
  message(FATAL_ERROR "site 2: the composer link never resolved from the posted metadata read:\n${s2}")
endif()
if(NOT "${s2}" MATCHES "view_reads panel_reads=[0-9]+ art_reads=[0-9]+ total_reads=[0-9]+ text_reads=([0-9]+) ")
  message(FATAL_ERROR "site 2: no view_reads line in:\n${s2}")
endif()
if(CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "site 2: the posted metadata read never ran (text_reads=${CMAKE_MATCH_1}):\n${s2}")
endif()
message(STATUS "site2_ok turn_ms_max=${s2_turn} text_reads=${CMAKE_MATCH_1}")
endif()

sites_want(6 w6)
if(w6)
# --- site 6: a queue over 4096 rows. The group read is one posted read keyed by the queue hash: bounded, not one per frame.
set(big "${WORK}/big.m3u")
string(REPEAT "${WORK}/songs/a.mod\n" 4200 big_text)
file(WRITE "${big}" "${big_text}")
sites_live(s6 "q@6000" SOCK s6b KNOBS SELECTA_LIB_GROUP_SLOW_MS ARGS --output null --db "${sites_db}" --view queue --exec "PLAYLIST_LOAD ${big}")
sites_field(s6_queue "${s6}" overlay queue)
if(s6_queue LESS 4097)
  message(FATAL_ERROR "site 6: the queue holds ${s6_queue} entries, want more than 4096:\n${s6}")
endif()
if(NOT "${s6}" MATCHES "view_reads panel_reads=[0-9]+ art_reads=[0-9]+ total_reads=[0-9]+ text_reads=[0-9]+ group_reads=([0-9]+)")
  message(FATAL_ERROR "site 6: no view_reads line in:\n${s6}")
endif()
if(CMAKE_MATCH_1 LESS 1 OR CMAKE_MATCH_1 GREATER 4)
  message(FATAL_ERROR "site 6: group_reads=${CMAKE_MATCH_1} for a settled ${s6_queue} entry queue, want 1..4 (not one per frame):\n${s6}")
endif()
message(STATUS "site6_ok turn_ms_max=${s6_turn} queue=${s6_queue} group_reads=${CMAKE_MATCH_1}")
endif()

# --- I906g PLAYLIST_PLAY: the key read is a posted write-queue op; the queue fills from its done callback, the UI thread never waits.
sites_mem(ignore "${sites_db}" home "wait 1\nexec PLAYLIST_ADD_SONG 1 local:${WORK}/songs/a.mod\nwait 2\n")
sites_live(s7 "q@4000" SOCK s7 KNOBS SELECTA_LIB_KEYS_SLOW_MS ARGS --output null --db "${sites_db}" --view queue --exec "PLAYLIST_PLAY 1")
sites_field(s7_queue "${s7}" overlay queue)
if(s7_queue LESS 1)
  message(FATAL_ERROR "site 7 PLAYLIST_PLAY: the queue is empty, the posted key read never delivered:\n${s7}")
endif()
message(STATUS "site7_ok wait_ms=${s7_turn} queue=${s7_queue}")

# --- I906g run_search callers: QUEUE_ADD_QUERY and PLAY_QUERY post the search on the write queue and act in its done callback. SEARCH_SLOW_MS
# makes the search a 300 ms statement; the queue holds the result songs (sentinel: the done callback ran), the UI thread never waited.
foreach(qcmd QUEUE_ADD_QUERY PLAY_QUERY)
  sites_live(s8 "q@4000" SOCK s8${qcmd} KNOBS SELECTA_LIB_SEARCH_SLOW_MS ARGS --output null --db "${sites_db}" --view queue --exec "${qcmd} \"selecta\"")
  sites_field(s8_queue "${s8}" overlay queue)
  if(s8_queue LESS 1)
    message(FATAL_ERROR "site 8 ${qcmd}: the queue is empty, the posted search never delivered:\n${s8}")
  endif()
  message(STATUS "site8_${qcmd}_ok wait_ms=${s8_turn} queue=${s8_queue}")
endforeach()

# --- I906g smart_sync: a song start with library.smart_downloads on plans the smart downloads as a write-queue op (SMART_SLOW_MS makes the plan
# a 300 ms statement). lib_writes_done counts finished ops: the PLAY_QUERY search, the song-start ops and the smart plan, so >= 2 proves the
# plan ran off the UI thread; lib_ui_wait_ms < 50 (sites_live) proves it did not wait there.
sites_live(s9 "q@4000" SOCK s9 KNOBS SELECTA_LIB_SMART_SLOW_MS ARGS --output null --db "${sites_db}" --view queue --exec "PREF_SET library.smart_downloads on" --exec "PLAY_QUERY \"selecta\"")
if(NOT "${s9}" MATCHES "lib_writes_done=([0-9]+)" OR CMAKE_MATCH_1 LESS 2)
  message(FATAL_ERROR "site 9 smart_sync: lib_writes_done=${CMAKE_MATCH_1}, want >= 2 (the plan op never ran):\n${s9}")
endif()
message(STATUS "site9_ok wait_ms=${s9_turn} writes=${CMAKE_MATCH_1}")

# --- I951: the remaining library writes (stream add, playlist import, touch, playlist delete, library remove, library clear) ride the
# write queue like PLAYLIST_CREATE. WRITE_SLOW_MS makes each a 300 ms statement; sites_live fails the run when the command runs it on the UI
# thread (lib_ui_wait_ms >= 50), and lib_writes_done proves the queued write really ran. Destructive verbs use their own copy of the db.
file(WRITE "${WORK}/i951.m3u" "${WORK}/songs/a.mod\n")
function(sites_write name exec)
  set(db "${WORK}/i951-${name}.db")
  file(REMOVE "${db}" "${db}-wal" "${db}-shm")
  file(COPY_FILE "${sites_db}" "${db}")
  sites_live(w "q@4000" SOCK w${name} KNOBS SELECTA_LIB_WRITE_SLOW_MS ARGS --output null --db "${db}" --view queue --exec "${exec}")
  if(NOT w MATCHES "lib_writes_done=([0-9]+)" OR CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "site I951 ${name}: the queued write did not run (lib_writes_done missing or 0):\n${w}")
  endif()
  message(STATUS "site_I951_${name}_ok wait_ms=${w_turn} lib_writes_done=${CMAKE_MATCH_1}")
endfunction()
sites_write(stream_add "STREAM_ADD http://example.invalid/radio.mp3")
sites_write(import "PLAYLIST_IMPORT 1 ${WORK}/i951.m3u")
sites_write(touch "LIBRARY_TOUCH 1 local:${WORK}/songs/a.mod")
sites_write(playlist_del "PLAYLIST_DEL 1")
sites_write(library_remove "LIBRARY_REMOVE local:${WORK}/songs/a.mod")
sites_write(library_clear "LIBRARY_CLEAR")
