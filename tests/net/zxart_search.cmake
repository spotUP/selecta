# Row I1024f: SOURCE_SEARCH zxart beyond, driven through selecta-cli against a local zxart.ee fixture.
#  1. a sync first downloads the people pages (the search names artists from them); then the engine command SOURCE_SEARCH asks the plugin,
#     which GETs /api/language:eng/export:zxMusic/start:0/limit:25/filter:zxMusicTitleSearch=beyond (fixture log); the answer has 2 tunes
#     = 2 result rows and a done event with 2;
#  2. sentinel SOURCESEARCH runs>=1 proves the new code ran; the rows carry the sync's key <id>/<file name> and the sync's
#     title, artist and format;
#  3. an unreachable site (closed port) reports search_failed (the plugin returned -1).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "zxart.ee=${fx},fixture.test=${fx}")
set(ENV{SELECTA_ZXART_SITE} "http://zxart.ee")
set(cache "${WORK}/zx-search-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/zx-search.db")
file(REMOVE "${db}")

function(need what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1024f ${what}: no match for '${pattern}'\n${text}")
  endif()
endfunction()

# I1231: the sync goes to its own db, so the live search below still runs (a synced zxart searches its rows, see the end)
set(synced "${WORK}/zx-search-synced.db")
file(REMOVE "${synced}")
execute_process(COMMAND "${CLI}" library sync --db "${synced}" --now 1792756800 zxart http://zxart.ee
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1024f: the sync that leaves the people pages failed rc=${rc}\n${o}${e}")
endif()
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
file(WRITE "${WORK}/zx-search.sel"
  "SOURCE_SEARCH zxart beyond\n@search_wait\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/zx-search.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
set(raw "${o}${e}")
message(STATUS "${raw}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1024f: script rc=${rc}\n${raw}")
endif()
need("sentinel: the search ran" "${raw}" "SOURCESEARCH runs=[1-9]")
# the row keeps its whole format name (I1043); the pattern checks the "Spectrum - Pro" prefix of it
need("row in the sync key shape, with title, artist, format" "${raw}"
  "zxart:19636/MmcM_-_Beyond_Time_\\(2013\\)[^\n]*Beyond Time[^\n]*MmcM[^\n]*Spectrum - Pro")
need("second row" "${raw}" "zxart:19717/Scalesmann_-_")
need("done event with the count" "${raw}" "search_done[^\n]*2")
string(REGEX MATCHALL "zxart:197?[0-9]+/" rows "${raw}")
list(REMOVE_DUPLICATES rows)
list(LENGTH rows nrows)
if(NOT nrows EQUAL 2 OR raw MATCHES "zxart:19718/")
  message(FATAL_ERROR "I1024f: want the two answered tunes as rows, got ${nrows}\n${raw}")
endif()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/zx-search-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/zx-search-log.json" log)
need("search request" "${log}" "export:zxMusic/start:0/limit:25/filter:zxMusicTitleSearch=beyond")

set(ENV{SELECTA_NET_MAP} "zxart.ee=http://127.0.0.1:9,fixture.test=${fx}")
file(WRITE "${WORK}/zx-dead.sel" "SOURCE_SEARCH zxart beyond\n@search_wait\n")
execute_process(COMMAND "${CLI}" script "${WORK}/zx-dead.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
need("unreachable site" "${o}${e}" "search_failed")

# I1231: in the synced db the search reads the synced rows (one local FTS query) and asks no site, so the closed port still answers
execute_process(COMMAND "${CLI}" script "${WORK}/zx-search.sel" --db "${synced}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
set(raw "${o}${e}")
if(raw MATCHES "search_failed")
  message(FATAL_ERROR "I1231: the synced zxart search went to the (closed) site\n${raw}")
endif()
need("synced row from the local catalogue" "${raw}" "zxart:19636/MmcM_-_Beyond_Time_\\(2013\\)[^\n]*MmcM")
need("no hook search: the local catalogue answered" "${raw}" "SOURCESEARCH runs=0")

message(STATUS "zxart_search_ok")
