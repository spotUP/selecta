# dui-topbar-search-from-source (I828): a click on the top bar search field gives it the keyboard from every page, also a source
# page. -DSELECTA -DSONGS -DWORK -DNETWORK=<net fixture work dir>
# Design doc: the top bar search field (I406a topbar search / I615 top bar parts), the field is the Search page's field.
# Flow through the real input path: the Sources chip (right chevron x5), the modulespl row of the Your Library list, a real click on the
# top bar field rect from the dump, then c h i p Enter against the recorded www.modules.pl fixture (no real network, no audio).
# Proof: nav page=search, the typed keys reach the field (focus), and `online scope=modulespl searches>=1 rows=3` (searches is
# the sentinel that the worker search ran). Before the fix the field click from a source page left the focus off the field.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},radio.modules.pl=${fx},fixture.test=${fx}")
# I1024a: archive.org is a searchable source now; point it at the fixture host (no advancedsearch route there,
# so 404 = search_failed, and no real network is touched)
set(ENV{SELECTA_ARCHIVEORG_BASE} "http://fixture.test")
# I1024d: radiobrowser searches too; same 404 = search_failed
set(ENV{SELECTA_RADIO_BROWSER} "http://fixture.test")
# I1024e: ccmixter searches too; same 404 = search_failed
set(ENV{SELECTA_CCMIXTER_BASE} "http://fixture.test")
# I1024b: jamendo searches too; same 404 = search_failed (no client id set: no request at all)
set(ENV{SELECTA_JAMENDO_API} "http://fixture.test")
unset(ENV{SELECTA_JAMENDO_CLIENT_ID})
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")

function(ts_run outvar view input)
  file(WRITE "${WORK}/in.txt" "${input}")
  file(REMOVE "${WORK}/library.db")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view ${view} --canvas 1280x1400 --clock fixed --stats
    --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# Every run starts from a fresh library (a source row click reorders the list by recency). I1019f: the scope is not remembered,
# the Search page opens on All sources and the Modules.pl chip is clicked after the typing.
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")

ts_run(c0 home "wait 300\n")
if(NOT "${c0}" MATCHES "sidechev L [0-9]+ [0-9]+ [0-9]+ [0-9]+ R ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no sidechev in:\n${c0}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ry "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(sources "")
foreach(k RANGE 1 5)
  string(APPEND sources "click ${rx} ${ry}\nwait 100\n")
endforeach()

# Pass 1: the Sources chip lists the registered sources; find the modulespl row.
ts_run(c1 home "wait 300\n${sources}wait 300\n")
if(NOT "${c1}" MATCHES "sidelib_row [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) [^\n]*[Mm]odules\\.?pl[^\n]*\n")
  message(FATAL_ERROR "no modulespl row under the Sources chip in:\n${c1}")
endif()
math(EXPR mx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR my "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(open "${sources}wait 300\nmove ${mx} ${my}\nclick ${mx} ${my}\nwait 300\n")

# Pass 2: the source page is open; read the top bar field rect there.
ts_run(c2 home "wait 300\n${open}")
if(NOT "${c2}" MATCHES "topbar draws=[0-9]+ shown=1 home [-0-9 ]+ field (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no top bar field on the source page in:\n${c2}")
endif()
math(EXPR fx0 "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR fy0 "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
if("${c2}" MATCHES "nav page=search")
  message(FATAL_ERROR "the modulespl row must open a source page, not Search:\n${c2}")
endif()

# The Your Library filter field holds the keyboard (magnifier click) when the user then clicks the top bar field.
if(NOT "${c2}" MATCHES "sidetools_rect mag (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ")
  message(FATAL_ERROR "no sidebar magnifier rect in:\n${c2}")
endif()
math(EXPR mx0 "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR my0 "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

# Pass 3: click the magnifier, then the field, type the query.
ts_run(c3 home "wait 300\n${open}click ${mx0} ${my0}\nwait 50\nmove ${fx0} ${fy0}\nclick ${fx0} ${fy0}\nwait 50\nkey c\nkey h\nkey i\nkey p\nkey enter\nwait 100\nwait 100\nwait 100\nwait 100\n")
if(NOT "${c3}" MATCHES "nav page=search")
  message(FATAL_ERROR "a click on the top bar field from a source page must open Search:\n${c3}")
endif()
if(NOT "${c3}" MATCHES "online scope=([^ ]*) searches=([0-9]+) rows=([0-9]+) ")
  message(FATAL_ERROR "no online line in:\n${c3}")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL "-")
  message(FATAL_ERROR "I1019f: the Search page must open on All sources (scope=-), got ${CMAKE_MATCH_1}:\n${c3}")
endif()
if(NOT "${c3}" MATCHES "online_chip 5 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no Modules.pl chip on the Search page:\n${c3}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

# Pass 4: the Modules.pl chip picked after the typing searches that source.
ts_run(c4 home "wait 300\n${open}click ${mx0} ${my0}\nwait 50\nmove ${fx0} ${fy0}\nclick ${fx0} ${fy0}\nwait 50\nkey c\nkey h\nkey i\nkey p\nkey enter\nwait 100\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 100\nwait 100\nwait 100\nwait 100\n")
if(NOT "${c4}" MATCHES "online scope=([^ ]*) searches=([0-9]+) rows=([0-9]+) ")
  message(FATAL_ERROR "no online line in:\n${c4}")
endif()
set(scope "${CMAKE_MATCH_1}")
set(searches "${CMAKE_MATCH_2}")
set(rows "${CMAKE_MATCH_3}")
if(NOT scope STREQUAL "modulespl" OR searches LESS 1 OR NOT rows EQUAL 3)
  message(FATAL_ERROR "typing chip after the field click and picking the chip must search modulespl (scope=${scope} searches=${searches} rows=${rows}, want modulespl >=1 3):\n${c4}")
endif()
