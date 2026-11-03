# Row I1024e: SOURCE_SEARCH ccmixter chip, driven through selecta-cli against a local /api/query fixture.
#  1. the engine command SOURCE_SEARCH asks the plugin, which GETs /api/query?...&search=chip (fixture log); the answer has
#     4 uploads, one under a non-open licence and one with only a WAV (the sync's filters drop them) = 2 result rows and a done
#     event with 2;
#  2. sentinel SOURCESEARCH runs>=1 proves the new code ran; the rows carry the sync's key <id>/<user>/<title>;
#  3. an unreachable service (closed port) reports search_failed (the plugin returned -1).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "cc.search.test=${fx},fixture.test=${fx},dead.search.test=http://127.0.0.1:9")
set(ENV{SELECTA_CCMIXTER_BASE} "http://cc.search.test")
set(cache "${WORK}/cc-search-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/cc-search.db")
file(REMOVE "${db}")

function(need what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1024e ${what}: no match for '${pattern}'\n${text}")
  endif()
endfunction()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
file(WRITE "${WORK}/cc-search.sel"
  "SOURCE_SEARCH ccmixter chip\n@search_wait\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/cc-search.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
set(raw "${o}${e}")
message(STATUS "${raw}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1024e: script rc=${rc}\n${raw}")
endif()
need("sentinel: the search ran" "${raw}" "SOURCESEARCH runs=[1-9]")
need("row path in the sync key shape" "${raw}" "ccmixter:201/searcher_one/Chip Remix")
need("done event with the filtered count" "${raw}" "search_done[^\n]*2")
string(REGEX MATCHALL "ccmixter:20[0-9]/" rows "${raw}")
list(REMOVE_DUPLICATES rows)
list(LENGTH rows nrows)
if(NOT nrows EQUAL 2 OR raw MATCHES "ccmixter:203/" OR raw MATCHES "ccmixter:204/")
  message(FATAL_ERROR "I1024e: want the two open MP3 uploads as rows (closed licence and WAV dropped), got ${nrows}\n${raw}")
endif()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/cc-search-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/cc-search-log.json" log)
need("search request" "${log}" "/api/query")
need("search query" "${log}" "search=chip")

set(ENV{SELECTA_CCMIXTER_BASE} "http://dead.search.test")
file(WRITE "${WORK}/cc-dead.sel" "SOURCE_SEARCH ccmixter chip\n@search_wait\n")
execute_process(COMMAND "${CLI}" script "${WORK}/cc-dead.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
need("unreachable service" "${o}${e}" "search_failed")

message(STATUS "ccmixter_search_ok")
