# T76 part 2 (row I167a): trim driven through LIBRARY_FETCH over fixture mirrors.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DM3=<tests/m3> -DCAT=<catalogue tsv>
# The cache is seeded with an old unmarked file. The fetched TFMX song is marked kept, the budget is tiny:
# the old file goes, the song and its smpl.* companion stay, and trim_runs proves fetch.c ran the trim.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "fixture.test=${fx},fast.mirror.test=${fx}")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODLAND} "http://fast.mirror.test/mods/")

set(cache "${WORK}/t76f-cache")
file(REMOVE_RECURSE "${cache}")
file(WRITE "${cache}/files/modland/Old/old.mod" "old unmarked file")
execute_process(COMMAND touch -t 202001010000 "${cache}/files/modland/Old/old.mod")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
execute_process(COMMAND "${CLI}" script "${M3}/cache_budget_fetch.sel" --catalogue "${CAT}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
set(last_raw "${o}${e}")

if(NOT last_raw MATCHES "(^|\n)trim_runs=([0-9]+)" OR CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "T76 fetch: trim did not run after LIBRARY_FETCH\n${last_raw}")
endif()
foreach(f TFMX/test/mdat.song TFMX/test/smpl.song)
  if(NOT EXISTS "${cache}/files/modland/${f}")
    message(FATAL_ERROR "T76 fetch: ${f} evicted although kept or companion of a kept song\n${last_raw}")
  endif()
endforeach()
if(EXISTS "${cache}/files/modland/Old/old.mod")
  message(FATAL_ERROR "T76 fetch: old unmarked file survived the trim\n${last_raw}")
endif()
message(STATUS "cache_budget_fetch_ok")
