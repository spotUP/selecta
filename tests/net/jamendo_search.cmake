# Row I1024b: SOURCE_SEARCH jamendo chip, driven through selecta-cli against a local /tracks fixture.
#  1. the engine command SOURCE_SEARCH asks the plugin, which GETs /tracks/?client_id=..&search=chip (fixture log); the answer has
#     3 tracks, one without an audio URL (the sync's filter drops it) = 2 result rows and a done event with 2;
#  2. sentinel SOURCESEARCH runs>=1 proves the new code ran; the rows carry the sync's key <id>/<artist>/<name>;
#  3. an unreachable service (closed port) reports search_failed (the plugin returned -1);
#  4. no client id: search_failed and NO request reaches the fixture.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "jam.search.test=${fx},fixture.test=${fx},dead.search.test=http://127.0.0.1:9")
set(ENV{SELECTA_JAMENDO_API} "http://jam.search.test")
set(ENV{SELECTA_JAMENDO_CLIENT_ID} "searchid1")
set(cache "${WORK}/jam-search-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/jam-search.db")
file(REMOVE "${db}")

function(need what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1024b ${what}: no match for '${pattern}'\n${text}")
  endif()
endfunction()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
file(WRITE "${WORK}/jam-search.sel"
  "SOURCE_SEARCH jamendo chip\n@search_wait\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/jam-search.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
set(raw "${o}${e}")
message(STATUS "${raw}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1024b: script rc=${rc}\n${raw}")
endif()
need("sentinel: the search ran" "${raw}" "SOURCESEARCH runs=[1-9]")
need("row path in the sync key shape" "${raw}" "jamendo:301/jam_one/Chip Dream")
need("done event with the filtered count" "${raw}" "search_done[^\n]*2")
string(REGEX MATCHALL "jamendo:30[0-9]/" rows "${raw}")
list(REMOVE_DUPLICATES rows)
list(LENGTH rows nrows)
if(NOT nrows EQUAL 2 OR raw MATCHES "jamendo:303/")
  message(FATAL_ERROR "I1024b: want the two tracks with audio as rows (no-audio dropped), got ${nrows}\n${raw}")
endif()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/jam-search-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/jam-search-log.json" log)
need("search request" "${log}" "/tracks/")
need("search query" "${log}" "search=chip")
need("client id" "${log}" "client_id=searchid1")

set(ENV{SELECTA_JAMENDO_API} "http://dead.search.test")
file(WRITE "${WORK}/jam-dead.sel" "SOURCE_SEARCH jamendo chip\n@search_wait\n")
execute_process(COMMAND "${CLI}" script "${WORK}/jam-dead.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
need("unreachable service" "${o}${e}" "search_failed")

# No client id: the plugin answers -1 before any request.
set(ENV{SELECTA_JAMENDO_API} "http://jam.search.test")
unset(ENV{SELECTA_JAMENDO_CLIENT_ID})
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
execute_process(COMMAND "${CLI}" script "${WORK}/jam-dead.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
need("no client id" "${o}${e}" "search_failed")
need("no client id line" "${o}${e}" "jamendo: no client id")
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/jam-search-log2.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/jam-search-log2.json" log2)
if("${log2}" MATCHES "/tracks/")
  message(FATAL_ERROR "I1024b: a search without a client id reached the network:\n${log2}")
endif()

message(STATUS "jamendo_search_ok")
