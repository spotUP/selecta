# dui-search-online (I406a): the desktop Search page searches an online source (Modules.pl), through the real shell and the
# scripted --input feeder, against the recorded www.modules.pl fixture (no real network, --output null: no audio).
# -DSELECTA -DSONGS -DWORK -DNETWORK=<net fixture work dir>
#  1. the Modules.pl scope chip + typed "chip" send SOURCE_SEARCH; the fixture answers 3 playable rows (`online ... rows=3`,
#     searches>=1 is the sentinel that the worker search ran);
#  2. a click on the first row plays it: plays=1 and the engine's `song` is modulespl:<path>;
#  3. the scope is remembered: a restart with the same config dir comes back with scope=modulespl (pref search.last_source);
#  4. a click on the Modules.pl Radio row counts radio_plays=1;
#  5. (I816) the fixture delays the first byte of the chip search by 4 s, past the client's 3 s default: rows=3 only
#     arrives because the plugin asks for the longer first-byte wait;
#  6. (I814) the modulespl SOURCES row is due=0 syncable=0.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},fixture.test=${fx}")
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
execute_process(COMMAND "${CMAKE_COMMAND}" -E true)

function(so_run outvar input)
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view search --canvas 1280x800 --clock fixed --stats
    --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(so_online outvar dump)
  if(NOT "${dump}" MATCHES "online scope=([^ ]*) searches=([0-9]+) rows=([0-9]+) drawn=([0-9]+) state=([0-9]+) plays=([0-9]+) radio_plays=([0-9]+)")
    message(FATAL_ERROR "no online line in:\n${dump}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1};${CMAKE_MATCH_2};${CMAKE_MATCH_3};${CMAKE_MATCH_4};${CMAKE_MATCH_5};${CMAKE_MATCH_6};${CMAKE_MATCH_7}" PARENT_SCOPE)
endfunction()

# Pass 1: find the Modules.pl chip (chip 0 is the library, chip 1 the first searchable source).
file(REMOVE_RECURSE "${WORK}/cfg")
file(REMOVE "${WORK}/library.db")
so_run(base "wait 300\n")
if(NOT "${base}" MATCHES "online_chip 1 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no online_chip 1 on the Search page:\n${base}")
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
if(NOT "${found}" MATCHES "online_radio (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no online_radio in:\n${found}")
endif()
math(EXPR rdx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR rdy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
if(${CMAKE_MATCH_3} EQUAL 0)
  message(FATAL_ERROR "the Modules.pl Radio row has no rectangle:\n${found}")
endif()

# Pass 3: click row 0.
file(REMOVE_RECURSE "${WORK}/cfg")
so_run(played "${typed}move ${rx} ${ry}\nclick ${rx} ${ry}\nwait 200\n")
so_online(on "${played}")
list(GET on 5 plays)
if(NOT plays EQUAL 1)
  message(FATAL_ERROR "a click on the online row must play it once (plays=${plays}):\n${played}")
endif()
if(NOT "${played}" MATCHES "song=([^\n]*)\n" OR NOT CMAKE_MATCH_1 MATCHES "${row_key}")
  message(FATAL_ERROR "the engine must open ${row_key}, got '${CMAKE_MATCH_1}':\n${played}")
endif()

# Pass 4: same config dir, no chip click: the scope comes back from the pref.
so_run(again "wait 300\n")
so_online(on "${again}")
list(GET on 0 scope)
if(NOT scope STREQUAL "modulespl")
  message(FATAL_ERROR "the scope must come back as modulespl after a restart, got '${scope}':\n${again}")
endif()

# Pass 5: the radio row.
file(REMOVE_RECURSE "${WORK}/cfg") # fresh config: the chips sit where pass 1 measured them
so_run(radio "${typed}move ${rdx} ${rdy}\nclick ${rdx} ${rdy}\nwait 100\n")
so_online(on "${radio}")
list(GET on 6 rp)
if(NOT rp EQUAL 1)
  message(FATAL_ERROR "a click on the Modules.pl Radio row must count radio_plays=1, got ${rp}:\n${radio}")
endif()
