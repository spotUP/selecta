# I1058e: opening an unfetched HippoPlayer list through the engine's posted read fetches its prg once and reloads the folder rows.
# Step 1: the CLI syncs the lists into a fresh db and cache against the hippoplayer.se fixture (lists known, prg not fetched).
# Step 2: the engine test opens that db with the same cache and reads one list's keys through sel_engine_playlist_keys_post/_take.
# -DCLI=<selecta-cli> -DTEST=<hippo_lazy_open_test> -DWORK=<fixture work dir>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "hippoplayer.se=${fx},fixture.test=${fx}")
set(cache "${WORK}/hippo-lazy-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(db "${WORK}/hippo-lazy.db")
file(REMOVE "${db}")
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
execute_process(COMMAND "${CLI}" library sync --db "${db}" --now 1792756800 hippo http://hippoplayer.se/
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "hippo lazy open: CLI sync failed\n${o}${e}")
endif()
execute_process(COMMAND "${TEST}" "${db}" "${WORK}/requests.jsonl" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
message(STATUS "${o}${e}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "hippo lazy open: engine test failed (rc ${rc})")
endif()
