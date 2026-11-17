# Row I1440i: the DeepSID composer photo as the cover of a SID song (design 2026-11-16-i1440-deepsid.md 2.3, D5), over the local hippoplayer.se fixture.
#  1. a known HVSC key with no other art: the photo (?w=320) becomes its cover, no Demozoo request;
#  2. a repeat sends no request (the cover is cached);
#  3. a key whose composer has no photo falls through: no cover, no image request;
#  4. revert proof: SELECTA_ART_SCRAPE=off stores nothing.
# Folder art and cover_url come first in dz_step by code order (library-folder-art covers the folder case).
# -DCLI=<selecta-cli> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "hippoplayer.se=${fx},fixture.test=${fx}")
set(ENV{SELECTA_DEEPSID_URL} "http://hippoplayer.se")
set(ENV{SELECTA_ART_DZ_URL} "http://hippoplayer.se")
unset(ENV{SELECTA_ART_SCRAPE})
set(hit "hvsc:MUSICIANS/H/Hubbard_Rob/Commando.sid")
set(miss "hvsc:MUSICIANS/N/Nobody/None.sid")
set(empty "${WORK}/dsc-lib")
file(MAKE_DIRECTORY "${empty}")

function(run db key out_var)
  execute_process(COMMAND "${CLI}" library scan --db "${db}" --demozoo "${key}" "${empty}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "I1440i: library scan --demozoo failed rc=${rc}\n${o}${e}")
  endif()
  set(${out_var} "${o}" PARENT_SCOPE)
endfunction()

function(want what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1440i ${what}: no match for '${pattern}' in\n${text}")
  endif()
endfunction()

function(count_requests pattern out_var)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/dsc-log.json" OUTPUT_VARIABLE lo)
  file(READ "${WORK}/dsc-log.json" log)
  string(REGEX MATCHALL "${pattern}" hits "${log}")
  list(LENGTH hits n)
  set(${out_var} ${n} PARENT_SCOPE)
endfunction()

count_requests("/api/deepsid/image/" img0)
count_requests("/api/demozoo/" dz0)
set(db "${WORK}/dsc.db")
file(REMOVE "${db}")
run("${db}" "${hit}" a)
want("photo is the cover" "${a}" "demozoo_shots=1 ")
count_requests("/api/deepsid/image/composers/Rob_Hubbard.jpg" i1)
count_requests("/api/demozoo/" dz1)
math(EXPR img1 "${i1} - 0")
if(dz1 GREATER dz0)
  message(FATAL_ERROR "I1440i: a Demozoo request was sent although the photo was the cover")
endif()
run("${db}" "${hit}" b)
count_requests("/api/deepsid/image/composers/Rob_Hubbard.jpg" i2)
count_requests("/api/deepsid/" d2)
run("${db}" "${hit}" b2)
count_requests("/api/deepsid/" d3)
if(NOT d3 EQUAL d2 OR i2 LESS 1)
  message(FATAL_ERROR "I1440i: repeat sent requests (before ${d2}, after ${d3}) or the photo was never requested (${i2})")
endif()

run("${db}" "${miss}" c)
want("no photo, no cover" "${c}" "demozoo_shots=1 ") # still the one cover of the hit key; the miss key adds none
count_requests("/api/deepsid/image/composers/" img3) # CSDb thumbnails (I1440l2) are image requests too
if(img3 GREATER i2)
  message(FATAL_ERROR "I1440i: an image request was sent for a key without a composer photo")
endif()

set(db2 "${WORK}/dsc-off.db")
file(REMOVE "${db2}")
set(ENV{SELECTA_ART_SCRAPE} "off")
run("${db2}" "${hit}" e)
want("off stores nothing" "${e}" "demozoo_shots=0 ")
message(STATUS "deepsid_cover_ok")
