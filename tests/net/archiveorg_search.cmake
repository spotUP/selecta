# Row I1024a: SOURCE_SEARCH archiveorg chip, driven through selecta-cli against a local advancedsearch/metadata fixture.
#  1. the engine command SOURCE_SEARCH asks the plugin, which GETs /advancedsearch.php with the query in a title/creator phrase and
#     the licence filter (fixture log); the answer has 11 hits: one without a licence (dropped, no metadata request) and 10 open
#     ones, of which only the first 8 get a /metadata request (SEARCH_ITEMS cap) = 8 result rows and a done event with 8;
#  2. sentinel SOURCESEARCH runs>=1 proves the new code ran; the rows carry the sync's key <id>/<creator>/<track>;
#  3. an unreachable service (closed port) reports search_failed (the plugin returned -1).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "ia.search.test=${fx},fixture.test=${fx},dead.search.test=http://127.0.0.1:9")
set(ENV{SELECTA_ARCHIVEORG_BASE} "http://ia.search.test")
set(cache "${WORK}/ia-search-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/ia-search.db")
file(REMOVE "${db}")

function(need what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1024a ${what}: no match for '${pattern}'\n${text}")
  endif()
endfunction()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
file(WRITE "${WORK}/ia-search.sel"
  "SOURCE_SEARCH archiveorg chip\n@search_wait\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/ia-search.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
set(raw "${o}${e}")
message(STATUS "${raw}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1024a: script rc=${rc}\n${raw}")
endif()
need("sentinel: the search ran" "${raw}" "SOURCESEARCH runs=[1-9]")
need("row path in the sync key shape" "${raw}" "archiveorg:ia_s01/Chip Artist 01/Chip Tune")
need("done event with the capped count" "${raw}" "search_done[^\n]*8")
string(REGEX MATCHALL "archiveorg:ia_s[0-9][0-9]/" rows "${raw}")
list(REMOVE_DUPLICATES rows)
list(LENGTH rows nrows)
if(NOT nrows EQUAL 8 OR raw MATCHES "ia_s09" OR raw MATCHES "ia_none")
  message(FATAL_ERROR "I1024a: want exactly the first 8 open hits as rows, got ${nrows}\n${raw}")
endif()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/ia-search-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/ia-search-log.json" log)
need("search request" "${log}" "/advancedsearch\\.php")
need("search query in a title phrase" "${log}" "title%3A%22chip%22")
need("licence filter" "${log}" "creativecommons\\.org")
string(REGEX MATCHALL "/metadata/ia_s[0-9][0-9]" metas "${log}")
list(LENGTH metas nmeta)
if(NOT nmeta EQUAL 8 OR log MATCHES "/metadata/ia_none" OR log MATCHES "/metadata/ia_s09")
  message(FATAL_ERROR "I1024a: want 8 metadata requests (cap), none for the unlicensed hit; got ${nmeta}\n${log}")
endif()

set(ENV{SELECTA_ARCHIVEORG_BASE} "http://dead.search.test")
file(WRITE "${WORK}/ia-dead.sel" "SOURCE_SEARCH archiveorg chip\n@search_wait\n")
execute_process(COMMAND "${CLI}" script "${WORK}/ia-dead.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
need("unreachable service" "${o}${e}" "search_failed")

message(STATUS "archiveorg_search_ok")
