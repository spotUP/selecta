# I718 hippo network part: `selecta-cli library sync hippo <url>` fetches /api/v1/playlists and /uhcplaylists.php
# into the cache and syncs them (the desktop Sync click sends the source home, https://hippoplayer.se/).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures>
# Main case: 2 requests, both lists kept, hippo_lists and db_songs equal the fixture JSON entry count.
# Bad case: a host answering 200 with an HTML page for both URLs keeps no file, reports sync_failed, 0 rows.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "hippoplayer.se=${fx},hippo.html.test=${fx},fixture.test=${fx}")

function(expect what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "hippo net ${what}: got '${got}', want '${want}'\n${raw}")
  endif()
endfunction()

function(counter var key)
  if(raw MATCHES "(^|[ \n])${key}=([^ \n]*)")
    set(${var} "${CMAKE_MATCH_2}" PARENT_SCOPE)
  else()
    set(${var} "" PARENT_SCOPE)
  endif()
endfunction()

file(READ "${FIXDIR}/hippoplayer.se/api/v1/playlists/index.json" listjson)
string(JSON want_lists LENGTH "${listjson}")

# ---- bad case first: HTML with 200
set(bad_cache "${WORK}/hippo-net-badcache")
file(REMOVE_RECURSE "${bad_cache}")
set(ENV{SELECTA_CACHE_DIR} "${bad_cache}")
set(bad_db "${WORK}/hippo-net-bad.db")
file(REMOVE "${bad_db}")
execute_process(COMMAND "${CLI}" library sync --db "${bad_db}" --now 1792756800 hippo http://hippo.html.test/
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
set(raw "${o}${e}")
file(GLOB_RECURSE kept "${bad_cache}/*hippo_list.json" "${bad_cache}/*uhcplaylists.tsv")
if(NOT "${kept}" STREQUAL "")
  message(FATAL_ERROR "hippo net: HTML 200 page kept as a list file: ${kept}\n${raw}")
endif()
if(NOT raw MATCHES "sync_failed")
  message(FATAL_ERROR "hippo net: HTML 200 pages did not fail the sync\n${raw}")
endif()
counter(v db_songs)
expect("rejected db_songs" "${v}" 0)

# ---- main case
set(cache "${WORK}/hippo-net-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/hippo-net.db")
file(REMOVE "${db}")
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
execute_process(COMMAND "${CLI}" library sync --db "${db}" --now 1792756800 hippo http://hippoplayer.se/
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
set(raw "${o}${e}")
expect("exit code" "${rc}" 0)
counter(v net_requests)
expect("net_requests (sentinel: the network step ran)" "${v}" 2)
counter(v hippo_lists)
# hippo_lists counts every list row read: the JSON entries plus the TSV rows (same lists, deduplicated to db_songs).
math(EXPR both "${want_lists} * 2")
expect("hippo_lists" "${v}" "${both}")
counter(v db_songs)
expect("db_songs" "${v}" "${want_lists}")
file(GLOB_RECURSE kept_json "${cache}/*hippo_list.json")
file(GLOB_RECURSE kept_tsv "${cache}/*uhcplaylists.tsv")
if("${kept_json}" STREQUAL "" OR "${kept_tsv}" STREQUAL "")
  message(FATAL_ERROR "hippo net: fetched lists not kept in the cache\n${raw}")
endif()

message(STATUS "hippo_net_ok")
