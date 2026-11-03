# dui-search-online (I406a): the desktop Search page searches an online source (Modules.pl), through the real shell and the
# scripted --input feeder, against the recorded www.modules.pl fixture (no real network, --output null: no audio).
# -DSELECTA -DSONGS -DWORK -DNETWORK=<net fixture work dir>
#  1. the Modules.pl scope chip + typed "chip" send SOURCE_SEARCH; the fixture answers 3 playable rows (`online ... rows=3`,
#     searches>=1 is the sentinel that the worker search ran);
#  2. a click on the first row plays it: plays=1 and the engine's `song` is modulespl:<path>;
#  3. the scope is remembered: a restart with the same config dir comes back with scope=modulespl (pref search.last_source);
#  4. (I1019h) the Search page has no radio entry; the Modules.pl source page has it and a click counts radio_plays=1;
#  5. (I816) the fixture delays the first byte of the chip search by 4 s, past the client's 3 s default: rows=3 only
#     arrives because the plugin asks for the longer first-byte wait;
#  5b. (I406b) playing a hit pins it: pins=1 pin_rows=1 (the sentinel that the library pin ran, 0 after a search alone),
#     and after a restart without a search the SOURCES row modulespl counts 1 song (the saved row);
#  6. (I814) the modulespl SOURCES row is due=0 syncable=0.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},radio.modules.pl=${fx},fixture.test=${fx}")
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
execute_process(COMMAND "${CMAKE_COMMAND}" -E true)

set(SO_CANVAS 1280x800)
function(so_run outvar input)
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view search --canvas ${SO_CANVAS} --clock fixed --stats
    --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(so_online outvar dump)
  if(NOT "${dump}" MATCHES "online scope=([^ ]*) searches=([0-9]+) rows=([0-9]+) drawn=([0-9]+) state=([0-9]+) plays=([0-9]+) radio_plays=([0-9]+)")
    message(FATAL_ERROR "no online line in:\n${dump}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4};${CMAKE_MATCH_5};${CMAKE_MATCH_6};${CMAKE_MATCH_7}" PARENT_SCOPE)
endfunction()

# Pass 1: find the Modules.pl chip (chip 0 is All sources, chip 1 the Library, chip 2 the first searchable source).
file(REMOVE_RECURSE "${WORK}/cfg")
file(REMOVE "${WORK}/library.db")
so_run(base "wait 300\n")
if(NOT "${base}" MATCHES "online_chip 2 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no online_chip 2 on the Search page:\n${base}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

# I814: Modules.pl is search-only (no sync hook): its SOURCES row is never due and the desktop draws no Sync for it.
if(NOT "${base}" MATCHES "source_sync [0-9]+ modulespl due=0 syncable=0\n")
  message(FATAL_ERROR "I814: the modulespl SOURCES row must be due=0 syncable=0:\n${base}")
endif()

# Pass 2: pick the chip, type the query, wait for the answer.
set(typed "wait 300\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 20\nkey c\nkey h\nkey i\nkey p\nkey enter\n")
foreach(i RANGE 1 3)
  string(APPEND typed "wait 100\n")
endforeach()
file(REMOVE_RECURSE "${WORK}/cfg")
so_run(found "${typed}")
so_online(on "${found}")
list(GET on 0 scope)
list(GET on 1 searches)
list(GET on 2 rows)
if(NOT scope STREQUAL "modulespl" OR searches LESS 1 OR NOT rows EQUAL 3)
  message(FATAL_ERROR "expected scope=modulespl searches>=1 rows=3, got '${on}':\n${found}")
endif()
if(NOT "${found}" MATCHES "online_row 0 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) (modulespl:[^\n]*)\n")
  message(FATAL_ERROR "no online_row 0 in:\n${found}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + 40")
math(EXPR ry "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(row_key "${CMAKE_MATCH_5}")
# I1019h: the Search page no longer offers the Modules.pl Radio entry (it lives on the Modules.pl source page).
if("${found}" MATCHES "online_radio")
  message(FATAL_ERROR "I1019h: the Search page must not draw a radio entry:\n${found}")
endif()

# Pass 3: click row 0.
file(REMOVE_RECURSE "${WORK}/cfg")
so_run(played "${typed}move ${rx} ${ry}\nclick ${rx} ${ry}\nwait 200\n")
so_online(on "${played}")
list(GET on 5 plays)
if(NOT plays EQUAL 1)
  message(FATAL_ERROR "a click on the online row must play it once (plays=${plays}):\n${played}")
endif()
# I406b: a search alone pins nothing; the played hit is pinned once and written to the library.
if(NOT "${found}" MATCHES "online_pin pins=0 pin_rows=0\n")
  message(FATAL_ERROR "I406b: a search alone must pin nothing:\n${found}")
endif()
if(NOT "${played}" MATCHES "online_pin pins=1 pin_rows=1\n")
  message(FATAL_ERROR "I406b: playing an online hit must pin it once (pins=1 pin_rows=1):\n${played}")
endif()
if(NOT "${played}" MATCHES "song=([^\n]*)\n" OR NOT CMAKE_MATCH_1 MATCHES "${row_key}")
  message(FATAL_ERROR "the engine must open ${row_key}, got '${CMAKE_MATCH_1}':\n${played}")
endif()

# Pass 4 (I1019f): same config dir, no chip click: the source filter is not remembered, the page opens on All sources.
so_run(again "wait 300\n")
so_online(on "${again}")
list(GET on 0 scope)
if(NOT scope STREQUAL "-")
  message(FATAL_ERROR "I1019f: the scope must not be remembered, got '${scope}' after a restart:\n${again}")
endif()

# I406b: the pinned row survived the restart: the modulespl source holds 1 song, and this run searched nothing.
if(NOT "${again}" MATCHES "source [0-9]+ modulespl\\|[^|]*\\|[^|]*\\|[0-9]+\\|1\n")
  message(FATAL_ERROR "I406b: after a restart the modulespl source must count the 1 pinned song:\n${again}")
endif()
if(NOT "${again}" MATCHES "online scope=- searches=0 ")
  message(FATAL_ERROR "I406b: the restart run must not search:\n${again}")
endif()

# Pass 5 (I1019h): the radio entry lives on the Modules.pl source page. The Search run above counted no radio play (a click where the
# entry used to sit plays the first hit); the pinned song of pass 3 gives the library a modulespl sidebar row, a click on it opens
# the source page, which draws the radio button (kindhead radio=x,y,w,h); a click on that counts radio_plays=1 and opens the stream.
so_online(on "${played}")
list(GET on 6 rp0)
if(NOT rp0 EQUAL 0)
  message(FATAL_ERROR "I1019h: Search must never count a radio play, got radio_plays=${rp0}:\n${played}")
endif()
set(SO_CANVAS 1280x1600) # the Modules.pl sidebar row sits below 800 px
so_run(side "wait 300\n")
if(NOT "${side}" MATCHES "sidelib_row [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=7 [Mm]odules[^\n]*\n")
  message(FATAL_ERROR "I1019h: no modulespl source row in the sidebar:\n${side}")
endif()
math(EXPR sx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR sy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
so_run(page "wait 300\nmove ${sx} ${sy}\nclick ${sx} ${sy}\nwait 300\n")
if(NOT "${page}" MATCHES "kindhead [^\n]*radio=(-?[0-9]+),(-?[0-9]+),([0-9]+),([0-9]+) ")
  message(FATAL_ERROR "I1019h: the modulespl source page has no radio button:\n${page}")
endif()
if(CMAKE_MATCH_3 EQUAL 0)
  message(FATAL_ERROR "I1019h: the modulespl source page radio button has no rectangle:\n${page}")
endif()
math(EXPR rdx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR rdy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
# The page open above touched Recents and reordered the sidebar, so the row is read again from an idle run right before the click.
so_run(side2 "wait 300\n")
if(NOT "${side2}" MATCHES "sidelib_row [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=7 [Mm]odules[^\n]*\n")
  message(FATAL_ERROR "I1019h: no modulespl source row in the sidebar (second read):\n${side2}")
endif()
math(EXPR sx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR sy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
so_run(radio "wait 300\nmove ${sx} ${sy}\nclick ${sx} ${sy}\nwait 300\nmove ${rdx} ${rdy}\nclick ${rdx} ${rdy}\nwait 1500\n")
so_online(on "${radio}")
list(GET on 6 rp)
if(NOT rp EQUAL 1)
  message(FATAL_ERROR "a click on the Modules.pl Radio button must count radio_plays=1, got ${rp}:\n${radio}")
endif()
# I815: the click reaches the real stream open path (listen.pls -> radio.modules.pl:8500/; with ICY metadata, joined mid-stream).
# The engine must open it: no open_failed toast, and the transport shows the stream playing.
if(NOT radio MATCHES "open_fail_toasts=0 ")
  message(FATAL_ERROR "the Modules.pl Radio stream failed to open (open_failed toast):\n${radio}")
endif()
if(NOT radio MATCHES "play_state=1")
  message(FATAL_ERROR "the Modules.pl Radio stream must be playing after the click:\n${radio}")
endif()
