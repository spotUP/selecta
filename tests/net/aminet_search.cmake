# Row I1024g: SOURCE_SEARCH aminet chip, driven through selecta-cli against a local aminet.net fixture.
#  1. the engine command SOURCE_SEARCH asks the plugin, which GETs /search?query=chip&path[]=mods&type=advanced&sort=name
#     (fixture log); the answer page has 3 rows, one of them an mpg the sync parser drops = 2 result rows and a done
#     event with 2;
#  2. sentinel SOURCESEARCH runs>=1 proves the new code ran; the rows carry the sync's key <dir>/<file name> and the
#     sync's name, artist (the description's "by") and format;
#  3. an unreachable site (closed port) reports search_failed (the plugin returned -1).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "aminet.net=${fx},fixture.test=${fx}")
set(ENV{SELECTA_AMINET_SITE} "http://aminet.net")
set(cache "${WORK}/am-search-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/am-search.db")
file(REMOVE "${db}")

function(need what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1024g ${what}: no match for '${pattern}'\n${text}")
  endif()
endfunction()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
file(WRITE "${WORK}/am-search.sel"
  "SOURCE_SEARCH aminet chip\n@search_wait\n@source_search\n")
execute_process(COMMAND "${CLI}" script "${WORK}/am-search.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
set(raw "${o}${e}")
message(STATUS "${raw}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1024g: script rc=${rc}\n${raw}")
endif()
need("sentinel: the search ran" "${raw}" "SOURCESEARCH runs=[1-9]")
need("row in the sync key shape, with name, artist, format" "${raw}"
  "aminet:mods/chip/AbsolutChips1.lha[^\n]*AbsolutChips1[^\n]*Sapphire[^\n]*Aminet module")
need("second row" "${raw}" "aminet:mods/chip/[A-Za-z0-9_-]+\\.(lha|lzh|zip)[^\n]*Cubik")
need("done event with the count" "${raw}" "search_done[^\n]*2")
string(REGEX MATCHALL "aminet:mods/[a-z]+/[A-Za-z0-9_.-]+" rows "${raw}")
list(REMOVE_DUPLICATES rows)
list(LENGTH rows nrows)
if(NOT nrows EQUAL 2 OR raw MATCHES "Almacen-Chip")
  message(FATAL_ERROR "I1024g: want the two archive rows (the mpg dropped), got ${nrows}\n${raw}")
endif()

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/am-search-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/am-search-log.json" log)
need("search request" "${log}" "/search\\?query=chip&path%5B%5D=mods&type=advanced&sort=name")

set(ENV{SELECTA_NET_MAP} "aminet.net=http://127.0.0.1:9,fixture.test=${fx}")
file(WRITE "${WORK}/am-dead.sel" "SOURCE_SEARCH aminet chip\n@search_wait\n")
execute_process(COMMAND "${CLI}" script "${WORK}/am-dead.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
need("unreachable site" "${o}${e}" "search_failed")

message(STATUS "aminet_search_ok")
