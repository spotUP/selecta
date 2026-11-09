# dui-source-page-search (I1229): the search field of a searchable source page searches that source, not only the page's loaded rows.
# Johan 2026-11-09 (Mac): "hvsc returns no hits for jammer now". I1224 gave every source page the collection list tools; its
# "Search in this collection" field sent COLLECTION_VIEW, which filters the few folder rows the HVSC page has loaded, so "jammer"
# found nothing although the HVSC catalogue holds Jammer songs. -DSELECTA -DSQLITE3 -DWORK
# Flow through the real input path: seeded library DB (two HVSC catalogue rows that match "jammer" by path and by artist, nothing in
# the page's loaded rows), the Sources chip (right chevron x5), the HVSC row of Your Library, a real click on the page's search field
# rect from the dump (collection ... find=x,y,w,h), then j a m m e r Enter.
# Proof: `col_source_search opens=1` (the sentinel: the field's click took the I1229 branch), nav page=search, and
# `online scope=hvsc searches>=1 rows=2` (the worker's catalogue search for HVSC ran and both seeded hits came back).
# Before the fix the run stays on the collection page with q=[jammer] shown=0 and the online search never runs.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(seed "${WORK}/seed.db")
dui_run(init --db "${seed}" --ui desktop --backend mem --view home --clock fixed)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${seed}"
  "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES ('hvsc:MUSICIANS/J/Jammer/Tune_A.sid','hvsc','MUSICIANS/J/Jammer/Tune_A.sid','Tune A','Ed','','PSID',507),('hvsc:v2.sid','hvsc','v2.sid','Tune B','Jan Sorensen (Jammer)','','PSID',508)"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seed failed: ${err}")
endif()

# Every run starts from the seeded DB (a source row click reorders the list by recency).
function(sp_run outvar input)
  file(WRITE "${WORK}/in.txt" "${input}")
  file(COPY_FILE "${seed}" "${WORK}/library.db")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view home --canvas 2000x1400 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

sp_run(c0 "wait 300\n")
if(NOT "${c0}" MATCHES "sidechev L [0-9]+ [0-9]+ [0-9]+ [0-9]+ R ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no sidechev in:\n${c0}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ry "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(sources "")
foreach(k RANGE 1 5)
  string(APPEND sources "click ${rx} ${ry}\nwait 100\n")
endforeach()

# Pass 1: the Sources chip lists the registered sources; find the HVSC row.
sp_run(c1 "wait 300\n${sources}wait 300\n")
if(NOT "${c1}" MATCHES "sidelib_row [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) [^\n]*HVSC[^\n]*\n")
  message(FATAL_ERROR "no HVSC row under the Sources chip in:\n${c1}")
endif()
math(EXPR mx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR my "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(open "${sources}wait 300\nmove ${mx} ${my}\nclick ${mx} ${my}\nwait 300\n")

# Pass 2: the HVSC source page is open; read its search field rect.
sp_run(c2 "wait 300\n${open}")
if(NOT "${c2}" MATCHES "nav page=collection")
  message(FATAL_ERROR "the HVSC row must open its source page:\n${c2}")
endif()
if(NOT "${c2}" MATCHES "collection kind=source [^\n]* find=(-?[0-9]+),(-?[0-9]+),([0-9]+),([0-9]+) ")
  message(FATAL_ERROR "no search field rect on the HVSC page in:\n${c2}")
endif()
math(EXPR kx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ky "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

# Pass 3: Johan's path: click the page's search field, type jammer.
sp_run(c3 "wait 300\n${open}move ${kx} ${ky}\nclick ${kx} ${ky}\nwait 50\nkey j\nkey a\nkey m\nkey m\nkey e\nkey r\nkey enter\nwait 300\nwait 300\nwait 300\nwait 300\nwait 300\nwait 300\n")
if(NOT "${c3}" MATCHES "col_source_search opens=1\n")
  message(FATAL_ERROR "I1229: a click on the HVSC page's search field must open the HVSC search (col_source_search opens=1):\n${c3}")
endif()
if(NOT "${c3}" MATCHES "nav page=search")
  message(FATAL_ERROR "I1229: the HVSC page's search field must open the Search page:\n${c3}")
endif()
if(NOT "${c3}" MATCHES "online scope=hvsc searches=[1-9][0-9]* rows=2 ")
  message(FATAL_ERROR "I1229: jammer typed in the HVSC page's search field must return the two HVSC hits (online scope=hvsc searches>=1 rows=2):\n${c3}")
endif()
