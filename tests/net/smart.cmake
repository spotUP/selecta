# T77 smart downloads (row I169): `selecta-cli script tests/m3/smart.sel` over fixture mirrors, five runs.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DM3=<tests/m3> -DCAT=<smart catalogue tsv>
# The history holds play2, the saved view holds song.mod, play1 is played. Default prefs: both are fetched in the
# background (smart_fetches=2). Each pref moves the number: smart_downloads off, metered on, smart_top 1, a full cache.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "fixture.test=${fx},fast.mirror.test=${fx}")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODLAND} "http://fast.mirror.test/mods/")

function(expect what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "T77 smart ${what}: got '${got}', want '${want}'\n${last_raw}")
  endif()
endfunction()

function(counter var key)
  if(last_raw MATCHES "(^|\n)${key}=([^\n]*)")
    set(${var} "${CMAKE_MATCH_2}" PARENT_SCOPE)
  else()
    set(${var} "" PARENT_SCOPE)
  endif()
endfunction()

# Runs the script with `pre` lines in front; sets last_raw.
function(run_smart name pre)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST
    OUTPUT_VARIABLE o RESULT_VARIABLE rc)
  # The history: the engine plays play2 to its end twice (SONG_ENDED writer, I170a) into the run's own DB, with a
  # throw-away cache so the smart plan below still has to download it.
  set(db "${WORK}/t77s-${name}.db")
  file(REMOVE "${db}")
  file(REMOVE_RECURSE "${WORK}/t77s-hist-${name}")
  set(ENV{SELECTA_CACHE_DIR} "${WORK}/t77s-hist-${name}")
  execute_process(COMMAND "${CLI}" script "${M3}/smart_history.sel" --catalogue "${CAT}" --db "${db}"
    OUTPUT_VARIABLE ho ERROR_VARIABLE he RESULT_VARIABLE hrc)
  if(NOT hrc EQUAL 0)
    message(FATAL_ERROR "T77 smart ${name}: history seed failed\n${ho}${he}")
  endif()
  set(cache "${WORK}/t77s-cache-${name}")
  file(REMOVE_RECURSE "${cache}")
  set(ENV{SELECTA_CACHE_DIR} "${cache}")
  file(READ "${M3}/smart.sel" body)
  file(WRITE "${WORK}/t77s-${name}.sel" "${pre}${body}")
  execute_process(COMMAND "${CLI}" script "${WORK}/t77s-${name}.sel" --catalogue "${CAT}" --db "${db}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  set(last_raw "${o}${e}" PARENT_SCOPE)
  set(cache_dir "${cache}" PARENT_SCOPE)
endfunction()

# A: defaults. The view song (song.mod) and the most played history song (play2) are downloaded behind the foreground.
run_smart(a "")
counter(v smart_fetches)
expect("A smart_fetches" "${v}" 2)
foreach(f play2.mod song.mod)
  if(NOT EXISTS "${cache_dir}/files/modland/Protracker/test/${f}")
    message(FATAL_ERROR "T77 smart: ${f} missing from the cache\n${last_raw}")
  endif()
endforeach()
counter(v fetches)
expect("A fetches (play1 + 2 smart)" "${v}" 3)
counter(v prefetch_fetches)
expect("A prefetch_fetches (no next entry)" "${v}" 0)

# B: library.smart_downloads off.
run_smart(b "PREF_SET library.smart_downloads off\n")
counter(v smart_fetches)
expect("B smart_fetches" "${v}" 0)
counter(v fetches)
expect("B fetches (play1 only)" "${v}" 1)

# C: metered connection.
run_smart(c "PREF_SET library.metered on\n")
counter(v smart_fetches)
expect("C smart_fetches" "${v}" 0)
counter(v fetches)
expect("C fetches (play1 only)" "${v}" 1)

# D: library.smart_top 1 takes only the view song.
run_smart(d "PREF_SET library.smart_top 1\n")
counter(v smart_fetches)
expect("D smart_fetches" "${v}" 1)
if(NOT EXISTS "${cache_dir}/files/modland/Protracker/test/song.mod")
  message(FATAL_ERROR "T77 smart D: the smart playlist song was not the one fetched\n${last_raw}")
endif()

# E: the cache is at its budget after the song, so nothing is fetched and the skips are counted.
run_smart(e "@budget 1000\n")
counter(v smart_fetches)
expect("E smart_fetches" "${v}" 0)
counter(v smart_skipped)
if(v STREQUAL "" OR v LESS 1)
  message(FATAL_ERROR "T77 smart E: no skipped smart download counted\n${last_raw}")
endif()

message(STATUS "smart_ok")
