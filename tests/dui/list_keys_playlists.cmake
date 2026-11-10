# dui-list-keys-playlists (I1275): every desktop list takes the same keys. Johan, Mac: "wtf list needs to have global key navigation..
# the playlists are not key navigatable". -DSELECTA -DSONGS -DWORK
#   Violated invariant: one list key handler (list_nav: Up, Down, Page Up, Page Down, Home, End) serves every list, and Enter opens or plays
#   the focus row. The sidebar playlist list kept its own copy without the page keys, and the song list of an opened playlist page
#   (col_kind 0) took no list keys at all (col_list_key returned for every page that is not a collection kind).
#   Driven through the desktop UI's real key entry (the scripted --input feeder, mem backend), on a library with one playlist of three songs.
# 1. Sidebar stop (one Tab), End: the cursor sits on the last row; Page Down from no cursor moves one page down and Page Up from the end one page up,
#    Home returns to row 0 (stat side_cursor = row + 1). The page keys were ignored before.
# 2. Sidebar stop, End, Enter opens the playlist page (the page a click on its row opens: `plpage id= name=[alpha] songs=3`).
# 3. Playlist page (focus already on it after Enter, no Tab), Down x2: the song list focus is row 1 (list_focus row 1), End row 2, Home row 0, Page Down row 2;
#    Enter plays the focus row (activates 1, plays 1, the engine's open song is that row's file).
# Sentinel: the `calls=` counter of the list_nav line counts the keys the ONE shared list_nav step took; it is 0 without keys and counts
# every key of the runs above (sidebar and playlist page both feed it).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n a b c)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/${n}.mod")
endforeach()
set(db "${WORK}/lib.db")
dui_run(seed --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}"
  --exec "QUEUE_ADD ${WORK}/a.mod" --exec "PLAYLIST_CREATE alpha"
  --exec "PLAYLIST_ADD_SONG 1 ${WORK}/b.mod" --exec "PLAYLIST_ADD_SONG 1 ${WORK}/c.mod")
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x800)

function(lk_run outvar keys)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/lk.txt" "${keys}")
  dui_run(o ${common} --input "${WORK}/lk.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(lk_nav out dump)
  if(NOT "${dump}" MATCHES "list_focus page ([0-9]+) row (-?[0-9]+) activates ([0-9]+) plays ([0-9]+) song=([^ \n]*)\n")
    message(FATAL_ERROR "no list_focus line in:\n${dump}")
  endif()
  set(${out}_row ${CMAKE_MATCH_2} PARENT_SCOPE)
  set(${out}_act ${CMAKE_MATCH_3} PARENT_SCOPE)
  set(${out}_plays ${CMAKE_MATCH_4} PARENT_SCOPE)
  set(${out}_song ${CMAKE_MATCH_5} PARENT_SCOPE)
  if(NOT "${dump}" MATCHES "list_nav calls=([0-9]+)")
    message(FATAL_ERROR "no list_nav line in:\n${dump}")
  endif()
  set(${out}_nav ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()

function(lk_expect what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

# no key: the sentinel is 0
lk_run(r0 "wait 5\n")
lk_nav(n0 "${r0}")
lk_expect("no key: shared list step calls" "${n0_nav}" 0)

# 1. the sidebar stop takes End, Page Down and Page Up
lk_run(se "key tab\nkey end\nwait 2\n")
dui_stat(se_cur "${se}" desktop side_cursor)
dui_stat(se_route "${se}" desktop focus_route_sidebar)
lk_nav(sen "${se}")
lk_expect("sidebar End routed" "${se_route}" 1)
lk_expect("sidebar End went through the shared list step" "${sen_nav}" 1)
if(se_cur LESS 3)
  message(FATAL_ERROR "sidebar End must reach the last row (stat >= 3), got ${se_cur}:\n${se}")
endif()
lk_run(sd "key tab\nkey pagedown\nwait 2\n")
dui_stat(sd_cur "${sd}" desktop side_cursor)
if(sd_cur LESS 2 OR NOT sd_cur LESS se_cur)
  message(FATAL_ERROR "sidebar Page Down must move one page down, between the first row and the last (stat ${sd_cur}, last ${se_cur})")
endif()
lk_run(su "key tab\nkey end\nkey pageup\nwait 2\n")
dui_stat(su_cur "${su}" desktop side_cursor)
if(su_cur LESS 1 OR NOT su_cur LESS se_cur)
  message(FATAL_ERROR "sidebar End then Page Up must move one page up from the last row (stat ${su_cur}, last ${se_cur})")
endif()
lk_nav(sun "${su}")
lk_expect("sidebar End and Page Up went through the shared list step" "${sun_nav}" 2)
lk_run(sh "key tab\nkey end\nkey home\nwait 2\n")
dui_stat(sh_cur "${sh}" desktop side_cursor)
lk_expect("sidebar End then Home lands on the first row (stat 1)" "${sh_cur}" 1)

# The cursor runs over the doors, then the Your Library rows in the order of the sidelib_row dump lines: alpha is cursor (doors + its row).
# doors = last cursor stat - items (the End run above reaches the last cursor, stat = doors + items).
if(NOT "${r0}" MATCHES "sidelib draws=[0-9]+ rows=[0-9]+ items=([0-9]+) ")
  message(FATAL_ERROR "no sidelib line in:\n${r0}")
endif()
math(EXPR doors "${se_cur} - ${CMAKE_MATCH_1}")
if(NOT "${r0}" MATCHES "sidelib_row ([0-9]+) [-0-9]+ [-0-9]+ [0-9]+ [0-9]+ pinned=[01] open=[01] kind=3 alpha")
  message(FATAL_ERROR "no alpha row in the sidebar dump:\n${r0}")
endif()
math(EXPR alpha_cursor "${doors} + ${CMAKE_MATCH_1}")
set(to_alpha "key tab\nkey home\n")
foreach(i RANGE 1 ${alpha_cursor})
  string(APPEND to_alpha "key down\n")
endforeach()
# 2. Home, Down to alpha, Enter opens the playlist page
lk_run(so "${to_alpha}key enter\nwait 20\n")
if(NOT "${so}" MATCHES "plpage id=1 name=\\[alpha\\] songs=3 ")
  message(FATAL_ERROR "sidebar Home, Down to alpha, Enter must open the playlist alpha with 3 songs:\n${so}")
endif()

# 3. the playlist page song list
set(open "${to_alpha}key enter\nwait 20\n") # I1290: no Tab after Enter, the opened page takes the keys at once
lk_run(p1 "${open}key down\nkey down\nwait 2\n")
lk_nav(p1n "${p1}")
lk_expect("playlist page Down x2 (first Down lands on row 0)" "${p1n_row}" 1)
lk_run(p2 "${open}key end\nwait 2\n")
lk_nav(p2n "${p2}")
lk_expect("playlist page End" "${p2n_row}" 2)
lk_run(p3 "${open}key end\nkey home\nwait 2\n")
lk_nav(p3n "${p3}")
lk_expect("playlist page Home" "${p3n_row}" 0)
lk_run(p4 "${open}key pagedown\nwait 2\n")
lk_nav(p4n "${p4}")
lk_expect("playlist page Page Down" "${p4n_row}" 2)
lk_run(p5 "${open}key down\nkey down\nkey enter\nwait 20\n")
lk_nav(p5n "${p5}")
lk_expect("playlist page Enter activates once" "${p5n_act}" 1)
lk_expect("playlist page Enter plays once" "${p5n_plays}" 1)
if(NOT "${p5}" MATCHES "plsong 1 [0-9]+ [0-9]+ [0-9]+ [0-9]+ file=([^ \n]*) ")
  message(FATAL_ERROR "no plsong 1 line in:\n${p5}")
endif()
lk_expect("playlist page Enter opens the focus row's song" "${p5n_song}" "${CMAKE_MATCH_1}")
if(p5n_nav LESS 2)
  message(FATAL_ERROR "the playlist page keys did not go through the shared list step (nav=${p5n_nav})")
endif()

# 4. I1290 (Johan, Mac: "movements stuck until i tab again to the next panel, then all keys flood in the middle panel"):
#    after Enter on a sidebar playlist the key focus is on the opened page (focus stat = main stop + 1) with no Tab: the keys above ran
#    without one. Only the opening Enter and the sidebar keys before it are routed to the sidebar stop (9 = Home, 7 Down, Enter).
lk_run(f1 "${open}key down\nkey down\nkey down\nwait 2\n")
lk_nav(f1n "${f1}")
dui_stat(f1_side "${f1}" desktop focus_route_sidebar)
dui_stat(f1_focus "${f1}" desktop focus)
dui_stat(f1_main "${f1}" desktop focus_route_main)
lk_expect("playlist page Down x3 without a Tab: the last row" "${f1n_row}" 2)
lk_expect("keys after the opening Enter are not routed to the sidebar stop" "${f1_side}" 9)
lk_expect("keys after the opening Enter are routed to the main stop" "${f1_main}" 3)
dui_stat(f1_stops "${f1}" desktop focus_moves)
lk_expect("one Tab only (to the sidebar), none after Enter" "${f1_stops}" 1)
if("${f1_focus}" STREQUAL "0")
  message(FATAL_ERROR "after Enter on a playlist the key focus must sit on the page stop, stat 0 means no stop:\n${f1}")
endif()
# 5. The same through the real live loop (timed key events), the keys landing while the page is still loading (40 ms after Enter):
#    each key must act once the rows are there, none dropped. Before the fix the first Down landed on the page with no rows and was lost.
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SOCKET} "${WORK}/lk.sock")
set(down k40000051)
set(timed "k09@1500 k4000004A@1700")
foreach(t RANGE 1900 3100 200)
  string(APPEND timed " ${down}@${t}")
endforeach()
string(APPEND timed " k0D@3300 ${down}@3320 ${down}@3340 ${down}@3360 q@6000")
file(REMOVE_RECURSE "${WORK}/cfg")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_SDL_TEST_TIMED} "${timed}")
execute_process(COMMAND "${SELECTA}" --config-dir "${WORK}/cfg" --db "${db}" --ui desktop --backend sdl --view home --canvas 1280x800 --clock fixed --stats
  OUTPUT_VARIABLE lo ERROR_VARIABLE le RESULT_VARIABLE lrc)
if(NOT lrc EQUAL 0)
  message(FATAL_ERROR "live run exited ${lrc}\n${le}")
endif()
lk_nav(ln "${lo}")
lk_expect("live: Down x3 pressed while the page loads reach row 2, none dropped" "${ln_row}" 2)
if(NOT "${lo}" MATCHES "list_nav calls=[0-9]+ held=([0-9]+)")
  message(FATAL_ERROR "no held= sentinel in:\n${lo}")
endif()
if(CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "live: no key was held for the loading page (held=${CMAKE_MATCH_1}): the test did not hit the load window")
endif()
message(STATUS "list-keys-playlists: sidebar cursor end=${se_cur} pageup=${su_cur}; playlist page rows ${p1n_row}/${p2n_row}/${p3n_row}/${p4n_row}, enter song ${p5n_song}")
