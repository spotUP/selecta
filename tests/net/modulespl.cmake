# Row I406: SOURCE_SEARCH modulespl chip, driven through selecta-cli against the recorded www.modules.pl answer.
#  1. the engine command SOURCE_SEARCH asks the plugin, which GETs /?id=search&q=chip (fixture log) and parses the page:
#     3 result events (one row of the recorded page has no playable file and is skipped) and one done event;
#     sentinel SOURCESEARCH runs>=1 proves the new code ran;
#  2. the last used search source is remembered (pref search.last_source);
#  3. OPEN modulespl:<result path> + PLAY fetches the zip, unpacks the one module member into the cache and plays it
#     (non-silent render; cached file is a module, not a zip);
#  4. an unknown source id reports an error and does not touch the network.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},fixture.test=${fx}")
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/") # the built-in mirror is https; the fixture is plain http
set(cache "${WORK}/i406-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/i406.db")
file(REMOVE "${db}" "${WORK}/i406.raw")

function(need what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I406 ${what}: no match for '${pattern}'\n${text}")
  endif()
endfunction()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)

# I406a: the zip is unpacked when LIBRARY_FETCH stores it (cache fetch), before any OPEN or song read.
set(fcache "${WORK}/i406f-cache")
file(REMOVE_RECURSE "${fcache}")
set(ENV{SELECTA_CACHE_DIR} "${fcache}")
file(WRITE "${WORK}/i406f.sel" "LIBRARY_FETCH modulespl:sixtus/Sixtus_-_--_Only_chip_--.zip\n")
execute_process(COMMAND "${CLI}" script "${WORK}/i406f.sel" --db "${WORK}/i406f.db"
  OUTPUT_VARIABLE fo ERROR_VARIABLE fe RESULT_VARIABLE frc TIMEOUT 40)
file(GLOB_RECURSE fcached "${fcache}/files/modulespl/*")
if(NOT fcached)
  message(FATAL_ERROR "I406a: LIBRARY_FETCH stored nothing\n${fo}${fe}")
endif()
foreach(f ${fcached})
  file(READ "${f}" fhead LIMIT 2 HEX)
  if(fhead STREQUAL "504b")
    message(FATAL_ERROR "I406a: ${f} is still a zip after LIBRARY_FETCH (unzip belongs to the fetch)")
  endif()
endforeach()
set(ENV{SELECTA_CACHE_DIR} "${cache}")
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)

file(WRITE "${WORK}/i406.sel"
  "SOURCE_SEARCH modulespl chip\n@search_wait\n@source_search\nPREF_GET search.last_source\n"
  "@dump ${WORK}/i406.raw\nOPEN modulespl:sixtus/Sixtus_-_--_Only_chip_--.zip\nPLAY\n@render 22050\n@sleep 600\n@render 22050\nSTOP\n"
  "SOURCE_SEARCH nosuchsource chip\n@search_wait\nSOURCE_SEARCH modulespl many\n@search_wait\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/i406.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 40)
set(raw "${o}${e}")
message(STATUS "${raw}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I406: script rc=${rc}\n${raw}")
endif()

need("sentinel: the search ran" "${raw}" "SOURCESEARCH runs=[1-9]")
need("row 1 path" "${raw}" "modulespl:sixtus/Sixtus_-_--_Only_chip_--\\.zip")
need("row 1 title" "${raw}" "-- Only chip --")
need("row 2 title (entity decoded)" "${raw}" "Chip 'o hoy")
need("row 3 path (raw ampersand)" "${raw}" "cooperation/Dan_&_Scorpik_-_Chip-chop\\.zip")
if(raw MATCHES "2HD")
  message(FATAL_ERROR "I406: the row without a playable file was delivered\n${raw}")
endif()
need("done event with the row count" "${raw}" "search_done[^\n]*3")
need("remembered source" "${raw}" "search.last_source[^\n]*modulespl")
need("unknown source is an error" "${raw}" "unknown_source")
# I406a: the search ran on the library job thread, not the command thread (I782), and was counted (STAT_SOURCE_SEARCHES).
need("worker-thread sentinel" "${raw}" "SOURCESEARCH runs=[1-9][0-9]* searches=[1-9][0-9]* worker=[1-9]")
# I406a (D-YT-4): 150 recorded hits arrive capped at 120 result rows plus one done event with the count 120.
string(REGEX MATCHALL "Many_[0-9][0-9][0-9]\\.zip" many "${raw}")
list(LENGTH many manyn)
if(NOT manyn EQUAL 120)
  message(FATAL_ERROR "I406a: ${manyn} result rows for 150 hits, want 120")
endif()
need("done event with the capped count" "${raw}" "search_done[^\n]*120")

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/i406-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/i406-log.json" log)
need("search request" "${log}" "id=search")
need("search query" "${log}" "q=chip")
need("zip request" "${log}" "/modules/sixtus/Sixtus_-_--_Only_chip_--\\.zip")

file(READ "${WORK}/i406.raw" hex HEX)
if(NOT hex MATCHES "[1-9a-f]")
  message(FATAL_ERROR "I406: the render is silent\n${raw}")
endif()
file(GLOB_RECURSE cached "${cache}/files/modulespl/*")
if(NOT cached)
  message(FATAL_ERROR "I406: no unpacked module in the cache\n${raw}")
endif()
foreach(f ${cached})
  file(READ "${f}" head LIMIT 2 HEX)
  if(head STREQUAL "504b")
    message(FATAL_ERROR "I406: ${f} is still a zip")
  endif()
endforeach()
# I406b: a hit that is played, queued or added to a playlist is pinned into the library (SOURCES row, Q1b). QUEUE_ADD hit 1,
# PLAYLIST_ADD_SONG hit 2, QUEUE_ADD hit 1 again: the library holds exactly 2 modulespl rows (the second add is an upsert, not
# a third row), and a second process on the same db finds both (the close drained the ring). pin_rows counts every row written.
set(pdb "${WORK}/i406b.db")
file(REMOVE "${pdb}")
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
file(WRITE "${WORK}/i406b.sel"
  "SOURCE_SEARCH modulespl chip\n@search_wait\n@source_search\n"
  "QUEUE_ADD modulespl:sixtus/Sixtus_-_--_Only_chip_--.zip\nPLAYLIST_CREATE pins\n"
  "PLAYLIST_ADD_SONG 1 \"modulespl:arachno/Arachno_-_Chip_'o_hoy.zip\"\n"
  "QUEUE_ADD modulespl:sixtus/Sixtus_-_--_Only_chip_--.zip\n@sleep 500\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/i406b.sel" --db "${pdb}"
  OUTPUT_VARIABLE po ERROR_VARIABLE pe RESULT_VARIABLE prc TIMEOUT 40)
set(praw "${po}${pe}")
message(STATUS "${praw}")
need("pin script rc" "${prc}" "^0$")
need("nothing pinned by a search alone" "${praw}" "SOURCESEARCH [^\n]*pins=0 pin_rows=0")
need("three pins written as 2 rows (sentinel: the library pin ran)" "${praw}" "SOURCESEARCH [^\n]*pins=3 pin_rows=3")
file(WRITE "${WORK}/i406c.sel" "LIBRARY_SEARCH chip\n@sleep 300\n@results\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/i406c.sel" --db "${pdb}"
  OUTPUT_VARIABLE qo ERROR_VARIABLE qe RESULT_VARIABLE qrc TIMEOUT 40)
set(qraw "${qo}${qe}")
message(STATUS "${qraw}")
need("a restart lists exactly the 2 pinned rows" "${qraw}" "RESULTS 2 ")
need("pinned row 1" "${qraw}" "modulespl:sixtus/Sixtus_-_--_Only_chip_--\\.zip")
need("pinned row 2" "${qraw}" "modulespl:arachno/Arachno_-_Chip_'o_hoy\\.zip")
need("the restart searched no source" "${qraw}" "SOURCESEARCH runs=0 ")

message(STATUS "modulespl_net_ok")
