# Row I1303c: Demozoo screenshots as the cover of a Modland song, driven through selecta-cli library scan --demozoo against a local
# hippoplayer.se lookup fixture (design 2026-11-09-artwork-scraper-design.md, "Demozoo slide show").
#  1. a matched key: demozoo_shots=3 and the fixture log holds the lookup plus three image requests;
#  2. an unmatched key: demozoo_miss=1 and one more lookup, no image request;
#  3. a repeat of both keys sends no request at all (the shots and the miss are cached);
#  4. revert proof: SELECTA_ART_SCRAPE=off sends nothing and stores nothing.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "hippoplayer.se=${fx},fixture.test=${fx}")
set(ENV{SELECTA_ART_DZ_URL} "http://hippoplayer.se")
unset(ENV{SELECTA_ART_SCRAPE})
set(hit "modland:pub/modules/Protracker/Skope/april in paris fix.mod")
set(miss "modland:pub/modules/Protracker/Nobody/no demo.mod")
set(empty "${WORK}/dz-lib")
file(MAKE_DIRECTORY "${empty}")

function(run db key out_var)
  execute_process(COMMAND "${CLI}" library scan --db "${db}" --demozoo "${key}" "${empty}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "I1303c: library scan --demozoo failed rc=${rc}\n${o}${e}")
  endif()
  set(${out_var} "${o}" PARENT_SCOPE)
endfunction()

function(want what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1303c ${what}: no match for '${pattern}' in\n${text}")
  endif()
endfunction()

function(count_requests out_var)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/dz-log.json" OUTPUT_VARIABLE lo)
  file(READ "${WORK}/dz-log.json" log)
  string(REGEX MATCHALL "/(api/demozoo|dz/)" hits "${log}")
  list(LENGTH hits n)
  set(${out_var} ${n} PARENT_SCOPE)
endfunction()

function(want_count what got expect)
  set(base ${base})
  math(EXPR rel "${got} - ${base}")
  if(NOT rel EQUAL expect)
    message(FATAL_ERROR "I1303c ${what}: expected ${expect} requests, saw ${rel}")
  endif()
endfunction()

count_requests(base) # the fixture log is shared and may hold an earlier run
set(db "${WORK}/dz.db")
file(REMOVE "${db}")
run("${db}" "${hit}" a)
want("shots cached" "${a}" "demozoo_shots=3 ")
want("sentinel requests" "${a}" "demozoo_requests=([1-9][0-9]*)")
count_requests(n1)
want_count("hit" ${n1} 4)

run("${db}" "${miss}" b)
want("miss cached" "${b}" "demozoo_miss=1 ")
count_requests(n2)
want_count("miss" ${n2} 5)

run("${db}" "${hit}" c)
run("${db}" "${miss}" d)
count_requests(n3)
want_count("repeats" ${n3} 5)

set(db2 "${WORK}/dz-off.db")
file(REMOVE "${db2}")
set(ENV{SELECTA_ART_SCRAPE} "off")
run("${db2}" "${hit}" e)
want("off stores nothing" "${e}" "demozoo_shots=0 ")
count_requests(n4)
want_count("scrape off" ${n4} 5)
message(STATUS "demozoo_art_ok")
