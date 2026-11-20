# Row I1465f: the GameBase64 screenshot of a game tune as a still (design 2026-11-20-i1465-i1468-source-independent-metadata.md D3a), over the local hippoplayer.se fixture.
#  1. a game tune (tag Game, application RELEASE): /api/deepsid/image/gb64/2001.png (the entry matching its title, not the first one) becomes a still;
#  2. a repeat sends no new gb64 request (cached by gb64 id);
#  3. a 404 (id 9999) is asked once, then not again within a day (gb64_miss);
#  4. a tune tagged Demo asks no gb64 image even with a gb64 field.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "hippoplayer.se=${fx},fixture.test=${fx}")
set(ENV{SELECTA_DEEPSID_URL} "http://hippoplayer.se")
set(ENV{SELECTA_ART_DZ_URL} "http://hippoplayer.se")
unset(ENV{SELECTA_ART_SCRAPE})
set(game "hvsc:MUSICIANS/G/Game_Co/Uridium.sid")
set(lost "hvsc:MUSICIANS/G/Game_Co/Lost.sid")
set(demo "hvsc:MUSICIANS/G/Game_Co/Demo.sid")
set(empty "${WORK}/gb64-lib")
file(MAKE_DIRECTORY "${empty}")
set(db "${WORK}/gb64.db")
file(REMOVE "${db}")

function(run key out_var)
  execute_process(COMMAND "${CLI}" library scan --db "${db}" --demozoo "${key}" "${empty}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "I1465f: library scan --demozoo failed rc=${rc}\n${o}${e}")
  endif()
  set(${out_var} "${o}" PARENT_SCOPE)
endfunction()

function(count_requests pattern out_var)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/gb64-log.json" OUTPUT_VARIABLE lo)
  file(READ "${WORK}/gb64-log.json" log)
  string(REGEX MATCHALL "${pattern}" hits "${log}")
  list(LENGTH hits n)
  set(${out_var} ${n} PARENT_SCOPE)
endfunction()

function(expect what got want)
  if(NOT got EQUAL want)
    message(FATAL_ERROR "I1465f ${what}: ${got} requests, wanted ${want}")
  endif()
endfunction()

count_requests("/api/deepsid/image/gb64/2001" base)
run("${game}" a)
if(NOT "${a}" MATCHES "demozoo_shots=[1-9]")
  message(FATAL_ERROR "I1465f: no still for the game tune in\n${a}")
endif()
count_requests("/api/deepsid/image/gb64/2001" r1)
count_requests("/api/deepsid/image/gb64/11606" wrong)
math(EXPR got "${r1} - ${base}")
expect("title match asks 2001 once" ${got} 1)
expect("first list entry 11606 never asked" ${wrong} 0)
run("${game}" b)
count_requests("/api/deepsid/image/gb64/2001" r2)
expect("repeat adds no gb64 request" ${r2} ${r1})

run("${lost}" c)
count_requests("/api/deepsid/image/gb64/9999" m1)
expect("404 asked once" ${m1} 1)
run("${lost}" d)
count_requests("/api/deepsid/image/gb64/9999" m2)
expect("404 not asked again within a day" ${m2} 1)

run("${demo}" e)
count_requests("/api/deepsid/image/gb64/2001" r3)
expect("a tune not tagged Game asks no gb64 image" ${r3} ${r1})

message(STATUS "deepsid_gb64_ok")
