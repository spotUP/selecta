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

file(WRITE "${WORK}/i406.sel"
  "SOURCE_SEARCH modulespl chip\n@source_search\nPREF_GET search.last_source\n"
  "@dump ${WORK}/i406.raw\nOPEN modulespl:sixtus/Sixtus_-_--_Only_chip_--.zip\nPLAY\n@render 22050\n@sleep 600\n@render 22050\nSTOP\n"
  "SOURCE_SEARCH nosuchsource chip\n")
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
message(STATUS "modulespl_net_ok")
