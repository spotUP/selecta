# Row I1127c: a remote song whose companion banks are on no mirror still plays, and the Now Playing state carries the
# bank-missing flag the panels show as "Sample bank not found".
# Mirror nobank.mirror.test (tests/net/fixtures) holds Mondschein.eup and none of its banks; fast.mirror.test holds both.
# (a) CLI: OPEN the nobank song, render, assert non-silent energy and a fixture log of 404s for fmtone2.fmb / piano.pmb.
# (b) selecta --stats: play the same key; `song_note bank_missing=1` (sentinel: the fetch path found the missing bank and the
#     engine published it); the same song through fast.mirror.test reports bank_missing=0 (the flag does not stick).
# (c) With the desktop UI (-DUI=desktop): the Now Playing dump line says note="Sample bank not found" drawn>=1.
# -DCLI -DSELECTA -DWORK -DM3 -DPLUGINS -DUI=<text|desktop>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "fast.mirror.test=http://127.0.0.1:${port},nobank.mirror.test=http://127.0.0.1:${port}")
set(ENV{SELECTA_PLUGIN_PATH} "${PLUGINS}")
unset(ENV{SELECTA_MIRROR})
set(key "modland:Euphony/Aya/Mondschein/Mondschein.eup")

function(play_stats tag host out)
  set(cache "${WORK}/i1127c-${tag}-cache")
  file(REMOVE_RECURSE "${cache}" "${WORK}/i1127c-${tag}-cfg")
  set(ENV{SELECTA_CACHE_DIR} "${cache}")
  set(ENV{SELECTA_MIRRORS_MODLAND} "http://${host}/mods/")
  file(WRITE "${WORK}/i1127c-${tag}.txt" "wait 1500\n")
  if("${UI}" STREQUAL "desktop")
    set(run_args --ui desktop --backend mem --canvas 1280x800 --clock fixed --input "${WORK}/i1127c-${tag}.txt")
  else()
    file(WRITE "${WORK}/i1127c-${tag}.keys" "wait 1500\nq\n")
    set(run_args --ui text --size 80x25 --keys "${WORK}/i1127c-${tag}.keys" --screen-dump "${WORK}/i1127c-${tag}.screen")
  endif()
  execute_process(COMMAND "${SELECTA}" --output null --config-dir "${WORK}/i1127c-${tag}-cfg"
    --db "${WORK}/i1127c-${tag}.db" ${run_args} --stats
    --exec "OPEN ${key}" --exec PLAY
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 90)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "I1127c ${tag}: selecta exited ${rc}\n${o}${e}")
  endif()
  set(${out} "${o}" PARENT_SCOPE)
endfunction()

# (a) non-silent playback with the banks missing.
set(cache "${WORK}/i1127c-cli-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(ENV{SELECTA_MIRRORS_MODLAND} "http://nobank.mirror.test/mods/")
set(raw "${WORK}/i1127c.raw")
file(REMOVE "${raw}" "${WORK}/i1127c-cli.db")
file(WRITE "${WORK}/i1127c.sel" "@dump ${raw}\nOPEN ${key}\nPLAY\n@render 441000\nSTOP\n")
execute_process(COMMAND "${CLI}" fetch http://fast.mirror.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
execute_process(COMMAND "${CLI}" script "${WORK}/i1127c.sel" --db "${WORK}/i1127c-cli.db"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 90)
if(NOT rc EQUAL 0 OR "${o}${e}" MATCHES "ERROR|open_failed")
  message(FATAL_ERROR "I1127c: the song did not play without its banks (exit ${rc})\n${o}${e}")
endif()
execute_process(COMMAND sh -c "od -An -v -t d2 '${raw}' | tr -s ' ' '\\n' | awk '$1 != \"\" { s += $1 * $1 } END { printf \"%d\", s }'"
  OUTPUT_VARIABLE energy)
if(energy LESS 1)
  message(FATAL_ERROR "I1127c: the bank-less song rendered silence (energy ${energy})")
endif()
execute_process(COMMAND "${CLI}" fetch http://fast.mirror.test/__fixture/log --out "${WORK}/i1127c-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/i1127c-log.json" log)
if(NOT log MATCHES "fmtone2.fmb" OR NOT log MATCHES "piano.pmb")
  message(FATAL_ERROR "I1127c: the bank requests never reached the mirror (the hook did not run)\n${log}")
endif()

# (b) the flag, with the banks missing and present.
play_stats(missing nobank.mirror.test o_missing)
if(NOT o_missing MATCHES "song_note bank_missing=1")
  message(FATAL_ERROR "I1127c: bank-less remote song does not carry bank_missing=1:\n${o_missing}")
endif()
play_stats(present fast.mirror.test o_present)
if(NOT o_present MATCHES "song_note bank_missing=0")
  message(FATAL_ERROR "I1127c: remote song with its banks carries bank_missing=1:\n${o_present}")
endif()

# (c) the desktop panel draws the note.
if("${UI}" STREQUAL "desktop")
  if(NOT o_missing MATCHES "nowplaying note=\"Sample bank not found\" drawn=([0-9]+)" OR CMAKE_MATCH_1 LESS 1)
    message(FATAL_ERROR "I1127c: the Now Playing panel did not draw the note:\n${o_missing}")
  endif()
  if(NOT o_present MATCHES "nowplaying note=\"\" drawn=0")
    message(FATAL_ERROR "I1127c: the note shows for a song whose banks are present:\n${o_present}")
  endif()
endif()
string(REGEX MATCH "song_note[^\n]*" n1 "${o_missing}")
string(REGEX MATCH "song_note[^\n]*" n2 "${o_present}")
message(STATUS "missing: ${n1}; present: ${n2}")
message(STATUS "remote_eup_missing_bank_ok energy=${energy}")
