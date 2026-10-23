# T77 part 2 (row I168a): `selecta-cli script tests/m3/prefetch_preempt.sel` over fixture mirrors.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DM3=<tests/m3> -DCAT=<catalogue tsv>
# slow.mirror.test answers slowpf.mod after 5 s. A foreground fetch issued while that prefetch is in flight must
# return within PREEMPT_BOUND_MS (1500); before I168a it waited on the cache lock for the rest of the 5 s.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "slow.mirror.test=${fx},fixture.test=${fx}")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODLAND} "http://slow.mirror.test/mods/")
set(PREEMPT_BOUND_MS 1500)

execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST
  OUTPUT_VARIABLE o RESULT_VARIABLE rc)
set(cache "${WORK}/t77p-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
execute_process(COMMAND "${CLI}" script "${M3}/prefetch_preempt.sel" --catalogue "${CAT}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
set(last_raw "${o}${e}")

if(NOT last_raw MATCHES "(^|\n)timed_ms=([0-9]+)")
  message(FATAL_ERROR "T77 preempt: no timed_ms in output\n${last_raw}")
endif()
set(ms "${CMAKE_MATCH_2}")
if(ms GREATER ${PREEMPT_BOUND_MS})
  message(FATAL_ERROR "T77 preempt: foreground fetch took ${ms} ms (bound ${PREEMPT_BOUND_MS}) behind a slow prefetch\n${last_raw}")
endif()
if(NOT last_raw MATCHES "(^|\n)prefetches=")
  message(FATAL_ERROR "T77 preempt: no cache report\n${last_raw}")
endif()
if(NOT EXISTS "${cache}/files/modland/Protracker/test/quick2.mod")
  message(FATAL_ERROR "T77 preempt: quick2.mod not in the cache\n${last_raw}")
endif()
if(EXISTS "${cache}/files/modland/Protracker/test/slowpf.mod")
  message(FATAL_ERROR "T77 preempt: the cancelled prefetch still stored slowpf.mod\n${last_raw}")
endif()
message(STATUS "prefetch_preempt_ok timed_ms=${ms}")
