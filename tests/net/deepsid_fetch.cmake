# Row I1440h: the DeepSID fetcher (Mac core) over the local hippoplayer.se fixture (design 2026-11-16-i1440-deepsid.md 2.2).
#  1. a known HVSC key: ds_file, ds_composer and ds_csdb rows, the tags as the sticker "deepsid.tags", three requests;
#  2. an unknown key: a ds_miss row after one request;
#  3. a repeat of both keys sends no request at all;
#  3b. I1450: an HTML 404 (route missing on the site) records no miss and is asked again; only the JSON not-found above is cached;
#  4. revert proof: SELECTA_ART_SCRAPE=off sends nothing and stores nothing.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "hippoplayer.se=${fx},fixture.test=${fx}")
set(ENV{SELECTA_DEEPSID_URL} "http://hippoplayer.se")
unset(ENV{SELECTA_ART_SCRAPE})
set(hit "hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid")
set(miss "hvsc:MUSICIANS/N/Nobody/None.sid")
set(empty "${WORK}/ds-lib")
file(MAKE_DIRECTORY "${empty}")

function(run db key out_var)
  execute_process(COMMAND "${CLI}" library scan --db "${db}" --deepsid "${key}" "${empty}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "I1440h: library scan --deepsid failed rc=${rc}\n${o}${e}")
  endif()
  set(${out_var} "${o}" PARENT_SCOPE)
endfunction()

function(want what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1440h ${what}: no match for '${pattern}' in\n${text}")
  endif()
endfunction()

set(db "${WORK}/ds.db")
file(REMOVE "${db}")
run("${db}" "${hit}" a)
want("hit rows" "${a}" "deepsid_file=1 deepsid_composer=1 deepsid_csdb=1 deepsid_miss=0 deepsid_tags=1 deepsid_requests=4") # file, composer, csdb + one CSDb release thumbnail (I1440l2)
run("${db}" "${miss}" b)
want("miss row" "${b}" "deepsid_file=1 deepsid_composer=1 deepsid_csdb=1 deepsid_miss=1 deepsid_tags=1 deepsid_requests=1")
run("${db}" "${hit}" c)
want("hit cached" "${c}" "deepsid_requests=0")
run("${db}" "${miss}" d)
want("miss cached" "${d}" "deepsid_requests=0")

set(html "hvsc:MUSICIANS/N/Nobody/Html.sid")
set(db3 "${WORK}/ds-html.db")
file(REMOVE "${db3}")
run("${db3}" "${html}" h1)
want("html 404 is no miss" "${h1}" "deepsid_miss=0 .*deepsid_requests=1")
run("${db3}" "${html}" h2)
want("html 404 is retried" "${h2}" "deepsid_miss=0 .*deepsid_requests=1")

set(db2 "${WORK}/ds-off.db")
file(REMOVE "${db2}")
set(ENV{SELECTA_ART_SCRAPE} "off")
run("${db2}" "${hit}" e)
want("off stores nothing" "${e}" "deepsid_file=0 deepsid_composer=0 deepsid_csdb=0 deepsid_miss=0 deepsid_tags=0 deepsid_requests=0")
message(STATUS "deepsid_fetch_ok")
