# dui-list-key-frames (I1151): Up/Down in a list move the highlight on screen, Tab moves the focus ring on screen. -DSELECTA -DWORK -DSDL_BUILT
# Bug (Johan, Mac, real library.db): "keyboard navigation seems mostly dead in the entire app", arrows in lists. The engine view's cursor moved
# but desk_on_event followed the view by a signature without the cursor, so nothing dirtied the UI and no frame was drawn for the key.
# Driven through the desktop UI's real live SDL loop (SELECTA_SDL_TEST_LIVE, timed key events), on the Home page of a fixture library with shelves.
# Three runs differ only in the keys: none, three Down (keycode 0x40000051), three Tab (0x09). Asserts:
# 1. the Down run draws at least 3 `frame end=` frames after the first key (one per key; the no-key run draws none);
# 2. view_cursor_repaints >= 3 in the Down run and 0 in the no-key run (the sentinel: the new repaint cause ran);
# 3. the Tab run draws at least 3 frames after the first key, and focus_moves is 3.
# Search page (Johan: "Down doesnt work at all" in the search results, Tab "gets stuck in the search input"):
# 4. typed query, Down, Down, Up: the first Down from the field puts the focus on the main region (focus=2, not the
#    sidebar's 1) and skips the group heading at row 0 to row 1, the next Down goes to row 2, Up back to row 1 (view_cursor=1);
#    search_skips counts the moves the heading-skip path sent, one per key (the sentinel, 3);
# 5. typed query, Tab: Tab leaves the field (focus_moves=1).
# Search list follows the cursor (I1161, Johan: "the list dont scroll with the selected line so i can navigate out of the visible area"):
# 6. a copy of the fixture with 80 extra songs named zq..., query "zq", 30 Down: the list scrolls (search_scroll > 0) and
#    search_follows counts one scroll-into-view per moved row (the sentinel, >= 30); the cursor row stays inside the list area.
# Keys follow the focus ring (I1164, Johan: "the focus border changes position but up/down still moves up/down in the list... so input is
# not following the focus ring"). Home page, Tab counts are focus stops (the focus stat is the stop + 1, 0 = no stop):
# 7. Tab to the sidebar stop, 3 Down: view_cursor stays at its start value and focus_route_sidebar counts the 3 keys (the sentinel);
# 8. Tab to the player stop, Down, Down, Up: the player: bindings VOLUME -5, -5, +5 leave the volume 5 under the no-key run (100, the
#    maximum, so the Up alone would not show) and focus_route_player is 3;
# 9. Tab to the main stop, 3 Down: view_cursor moves as in the unfocused run and focus_route_main counts the 3 keys.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-list-key-frames needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_home_shelves.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SOCKET} "${WORK}/dui-list-key-frames.sock")

function(live tag timed)
  set(view home)
  if(ARGC GREATER 2)
    set(view ${ARGV2})
  endif()
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(MAKE_DIRECTORY "${WORK}/cfg")
  file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
  set(ENV{SELECTA_SDL_TEST_TIMED} "${timed}")
  execute_process(COMMAND "${SELECTA}" --config-dir "${WORK}/cfg" --db "${db}" --ui desktop --backend sdl --view ${view} --canvas 1280x800 --clock fixed --stats
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${tag}: selecta exited ${rc}\nstdout:\n${o}\nstderr:\n${e}")
  endif()
  # The first key comes 2000 ms after the window opened; frames ended 1900 ms or more after the first frame were drawn for a key.
  # The cut uses the `frame end=` clock itself: `trace t_ms=` counts from main entry (I625a), `frame end=` is the host clock.
  string(REGEX MATCHALL "frame end=[0-9]+" fr "${e}")
  if(NOT fr)
    message(FATAL_ERROR "${tag}: no frame end= line in:\n${e}")
  endif()
  list(GET fr 0 f0)
  string(REPLACE "frame end=" "" f0 "${f0}")
  math(EXPR cut "${f0} + 1900")
  set(nfr 0)
  foreach(f IN LISTS fr)
    string(REPLACE "frame end=" "" t "${f}")
    if(NOT t LESS cut)
      math(EXPR nfr "${nfr} + 1")
    endif()
  endforeach()
  if(NOT "${o}" MATCHES "view_cursor_repaints=([0-9]+)")
    message(FATAL_ERROR "${tag}: no view_cursor_repaints in:\n${o}")
  endif()
  set(${tag}_frames ${nfr} PARENT_SCOPE)
  set(${tag}_repaints ${CMAKE_MATCH_1} PARENT_SCOPE)
  foreach(k focus_moves view_cursor focus search_skips search_scroll search_follows volume focus_route_main focus_route_player focus_route_sidebar focus_route_detail)
    if("${o}" MATCHES " ${k}=([0-9]+)")
      set(${tag}_${k} ${CMAKE_MATCH_1} PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

live(none "q@5000")
live(down "k40000051@2000 k40000051@2600 k40000051@3200 q@5000")
live(tab "k9@2000 k9@2600 k9@3200 q@5000")
message(STATUS "frames none=${none_frames} down=${down_frames} tab=${tab_frames}; repaints none=${none_repaints} down=${down_repaints}; tab moves=${tab_focus_moves}")
set(down_extra ${down_frames})
set(tab_extra ${tab_frames})
if(NOT none_frames EQUAL 0 OR NOT none_repaints EQUAL 0)
  message(FATAL_ERROR "no key pressed but ${none_frames} frames after start-up and view_cursor_repaints=${none_repaints}")
endif()
if(down_repaints LESS 3)
  message(FATAL_ERROR "3 Down keys must repaint for the moved view cursor at least 3 times, view_cursor_repaints=${down_repaints}")
endif()
if(down_extra LESS 3)
  message(FATAL_ERROR "3 Down keys drew only ${down_extra} frames after the first key: the highlight does not move on screen")
endif()
if(NOT tab_focus_moves EQUAL 3 OR tab_extra LESS 3)
  message(FATAL_ERROR "3 Tab keys must move the focus 3 times (${tab_focus_moves}) and draw at least 3 frames after the first key (${tab_frames})")
endif()

# 7-9. (I1164) arrow keys act on the focused stop only.
live(fside "k9@2000 k40000051@2600 k40000051@3200 k40000051@3800 q@5000")
live(fplay "k9@2000 k9@2200 k9@2400 k9@2600 k40000051@3200 k40000051@3800 k40000052@4400 q@6000")
live(fmain "k9@2000 k9@2400 k40000051@2600 k40000051@3200 k40000051@3800 q@5000")
message(STATUS "focus routes: sidebar focus=${fside_focus} cursor=${fside_view_cursor} route=${fside_focus_route_sidebar}; player focus=${fplay_focus} volume=${fplay_volume} (none ${none_volume}) route=${fplay_focus_route_player}; main focus=${fmain_focus} cursor=${fmain_view_cursor} route=${fmain_focus_route_main}")
if(NOT fside_focus EQUAL 1 OR NOT fside_focus_route_sidebar EQUAL 3 OR NOT fside_view_cursor EQUAL none_view_cursor)
  message(FATAL_ERROR "3 Down on the sidebar stop must not move the page list: focus=${fside_focus} (want 1) route_sidebar=${fside_focus_route_sidebar} (want 3) view_cursor=${fside_view_cursor} (want ${none_view_cursor})")
endif()
math(EXPR want_vol "${none_volume} - 5") # the volume starts at its 100 maximum: Down, Down, Up is -5 -5 +5
if(NOT fplay_focus_route_player EQUAL 3 OR NOT fplay_volume EQUAL want_vol OR NOT fplay_view_cursor EQUAL none_view_cursor)
  message(FATAL_ERROR "Up on the player stop must send VOLUME -5, -5, +5: route_player=${fplay_focus_route_player} (want 3) volume=${fplay_volume} (want ${want_vol}) view_cursor=${fplay_view_cursor} (want ${none_view_cursor})")
endif()
if(NOT fmain_focus_route_main EQUAL 3 OR NOT fmain_view_cursor EQUAL down_view_cursor OR fmain_view_cursor EQUAL none_view_cursor)
  message(FATAL_ERROR "3 Down on the main stop must move the page list as without a focus: route_main=${fmain_focus_route_main} (want 3) view_cursor=${fmain_view_cursor} (want ${down_view_cursor}, not ${none_view_cursor})")
endif()

live(sdown "ta@2000 k40000051@2600 k40000051@3200 k40000052@3800 q@5500" search)
live(stab "ta@2000 k9@2600 q@4500" search)
message(STATUS "search: focus=${sdown_focus} view_cursor=${sdown_view_cursor} search_skips=${sdown_search_skips}; tab focus_moves=${stab_focus_moves}")
if(NOT sdown_focus EQUAL 2)
  message(FATAL_ERROR "Down from the search field must focus the main region (focus=2), focus=${sdown_focus}")
endif()
if(NOT sdown_view_cursor EQUAL 1 OR NOT sdown_search_skips EQUAL 3)
  message(FATAL_ERROR "Down, Down, Up from the search field must move row 1 (past the heading), row 2, row 1: view_cursor=${sdown_view_cursor} search_skips=${sdown_search_skips} (want 1 and 3)")
endif()
if(NOT stab_focus_moves EQUAL 1)
  message(FATAL_ERROR "Tab in the search field must move the focus (focus_moves=${stab_focus_moves})")
endif()

# 6. (I1161) the Search list scrolls with the keyboard cursor.
set(db_main "${db}")
set(db "${WORK}/lib_many.db")
file(COPY_FILE "${db_main}" "${db}")
execute_process(COMMAND "${SQLITE3}" "${db}"
  "WITH RECURSIVE n(i) AS (SELECT 0 UNION ALL SELECT i+1 FROM n WHERE i<79) INSERT INTO songs(key, source, path, name, artist, album, added) SELECT 'local:zq'||i||'.mod','local','zq'||i||'.mod','zq'||i,'Zed','ZAlb',1000+i FROM n;"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "adding the extra songs failed: ${err}")
endif()
set(keys "tzq@2000")
foreach(i RANGE 0 29)
  math(EXPR t "2600 + ${i} * 120")
  string(APPEND keys " k40000051@${t}")
endforeach()
string(APPEND keys " q@7500")
live(sfollow "${keys}" search)
message(STATUS "search follow: view_cursor=${sfollow_view_cursor} search_scroll=${sfollow_search_scroll} search_follows=${sfollow_search_follows}")
if(NOT sfollow_view_cursor GREATER 20)
  message(FATAL_ERROR "30 Down keys in an 80-row search list must move the cursor past row 20, view_cursor=${sfollow_view_cursor}")
endif()
if(NOT sfollow_search_follows GREATER_EQUAL 30)
  message(FATAL_ERROR "every Down must run the scroll-into-view step, search_follows=${sfollow_search_follows} (want >= 30)")
endif()
if(NOT sfollow_search_scroll GREATER 0)
  message(FATAL_ERROR "the cursor moved to row ${sfollow_view_cursor} but the search list did not scroll (search_scroll=${sfollow_search_scroll})")
endif()
