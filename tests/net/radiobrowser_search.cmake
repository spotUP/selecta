# Row I1024d: SOURCE_SEARCH radiobrowser chip, driven through selecta-cli against a local /json/stations/search fixture.
#  1. the engine command SOURCE_SEARCH asks the plugin, which GETs /json/stations/search?name=chip (fixture log); the answer has
#     3 stations, one AAC (the sync's codec filter drops it) = 2 result rows and a done event with 2;
#  2. sentinel SOURCESEARCH runs>=1 proves the new code ran; the rows carry the sync's key <uuid>/<name>;
#  3. an unreachable service (closed port) reports search_failed (the plugin returned -1).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "rb.search.test=${fx},fixture.test=${fx},dead.search.test=http://127.0.0.1:9")
set(ENV{SELECTA_RADIO_BROWSER} "http://rb.search.test")
set(cache "${WORK}/rb-search-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/rb-search.db")
file(REMOVE "${db}")

function(need what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1024d ${what}: no match for '${pattern}'\n${text}")
  endif()
endfunction()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
file(WRITE "${WORK}/rb-search.sel"
  "SOURCE_SEARCH radiobrowser chip\n@search_wait\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/rb-search.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
set(raw "${o}${e}")
message(STATUS "${raw}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1024d: script rc=${rc}\n${raw}")
endif()
need("sentinel: the search ran" "${raw}" "SOURCESEARCH runs=[1-9]")
need("row path in the sync key shape" "${raw}" "radiobrowser:rs-0001/Chiptune Radio One")
need("done event with the filtered count" "${raw}" "search_done[^\n]*2")
string(REGEX MATCHALL "radiobrowser:rs-000[0-9]/" rows "${raw}")
list(REMOVE_DUPLICATES rows)
list(LENGTH rows nrows)
if(NOT nrows EQUAL 2 OR raw MATCHES "rs-0003")
  message(FATAL_ERROR "I1024d: want the MP3 and the Ogg station as rows (AAC dropped), got ${nrows}\n${raw}")
endif()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/rb-search-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/rb-search-log.json" log)
need("search request" "${log}" "/json/stations/search")
need("search query as name" "${log}" "name=chip")

set(ENV{SELECTA_RADIO_BROWSER} "http://dead.search.test")
file(WRITE "${WORK}/rb-dead.sel" "SOURCE_SEARCH radiobrowser chip\n@search_wait\n")
execute_process(COMMAND "${CLI}" script "${WORK}/rb-dead.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
need("unreachable service" "${o}${e}" "search_failed")

message(STATUS "radiobrowser_search_ok")
