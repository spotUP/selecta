# dui-open-async (I1225): a song open never blocks the desktop UI thread on the network.
# -DSELECTA -DWORK -DFIXDIR -DPLUGINS=<dir holding sel_sysfile_probe>
# Research: thoughts/shared/research/2026-11-09-I1225-ui-blocking-open.md.
# Before: read_song ran sel_library_fetch (the whole remote download into the file cache) inside prepare_slot on the pump
# thread, so clicking a cold hvsc/modland song froze the window for the length of the download (Johan: beach ball).
# Now the I906g song-start read (rd_play, play_meta_run) fetches the remote file off the pump; the start waits deferred
# like it waits for its library read, and prepare_slot finds a cache hit.
# Driven through the desktop UI's live SDL loop (SDL_VIDEODRIVER=dummy, paced null output, no audio).
# Leg (a): OPEN modland:Protracker/test/slowplay.mod, slow.mirror.test answers after 4 s.
# Leg (b): the sysfile_probe replayer's system file arrives after 3 s (mirror.sysfile.test/sysfiles/slow.bin).
# Asserts per leg:
# 1. pump_ms_max < OPEN_ASYNC_BOUND_MS: the pump never sat in the 3-4 s transfer;
# 2. sentinel: (a) open_fetch_reads>=1 and songs_opened=1 (the fetch ran on the read slot and the song started);
#    (b) sysfile installed=1 reopens=1 (the download landed and the song re-opened).
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(OPEN_ASYNC_BOUND_MS 250)
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_NULL_PACED} 1)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "slow.mirror.test=${fx},mirror.sysfile.test=${fx},fixture.test=${fx}")
set(ENV{SELECTA_MIRRORS_MODLAND} "http://slow.mirror.test/mods/")
set(ENV{SELECTA_PLUGIN_PATH} "${PLUGINS}")
unset(ENV{SELECTA_MIRROR})
set(good "${FIXDIR}/mirror.sysfile.test/sysfiles/good.bin")
file(SHA256 "${good}" sha)
set(ENV{SELECTA_PROBE_SYSFILE_SHA} "${sha}")
set(ENV{SELECTA_PROBE_SYSFILE_URLS} "http://mirror.sysfile.test/sysfiles/slow.bin")
set(root "${WORK}/open-async")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")
file(WRITE "${root}/probe.sfp" "SFP\n")

function(run_leg tag song out)
  if(NOT DEFINED TIMED)
    set(TIMED "q@8000")
  endif()
  set(cfg "${root}/${tag}-cfg")
  file(MAKE_DIRECTORY "${cfg}")
  file(WRITE "${cfg}/prefs.conf" "library.sync_stale_days=0\n")
  set(ENV{SELECTA_CACHE_DIR} "${root}/${tag}-cache")
  set(ENV{SELECTA_SDL_TEST_TIMED} "${TIMED}")
  execute_process(COMMAND "${SELECTA}" --output null --config-dir "${cfg}" --db "${root}/${tag}.db" --ui desktop --backend sdl
    --view home --canvas 1280x800 --clock fixed --stats --exec "OPEN ${song}" --exec PLAY
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "open-async ${tag}: selecta exited ${rc}\n${o}${e}")
  endif()
  if(NOT "${o}" MATCHES "ui_stats_pump id=desktop pump_ms_max=([0-9]+)")
    message(FATAL_ERROR "open-async ${tag}: no ui_stats_pump line in:\n${o}${e}")
  endif()
  set(pump ${CMAKE_MATCH_1})
  if("${o}" MATCHES "open_phases [^\n]*")
    message(STATUS "open-async ${tag}: ${CMAKE_MATCH_0} pump_ms_max=${pump}")
  endif()
  if(pump GREATER_EQUAL ${OPEN_ASYNC_BOUND_MS})
    message(FATAL_ERROR "open-async ${tag}: pump_ms_max is ${pump}, want < ${OPEN_ASYNC_BOUND_MS} (the open waited on the network on the UI thread)\n${o}")
  endif()
  # --clock fixed freezes the UI frame clock, so pump_ms_max alone can read 0 while a pump
  # sat in a transfer. open_prepare_ms_max is measured with the host wall clock around
  # begin_song's prepare_slot, which runs on the pump (UI) thread: bound it as well.
  if(NOT "${o}" MATCHES "open_prepare_ms_max=([0-9]+)")
    message(FATAL_ERROR "open-async ${tag}: no open_prepare_ms_max in:\n${o}")
  endif()
  if(CMAKE_MATCH_1 GREATER_EQUAL ${OPEN_ASYNC_BOUND_MS})
    message(FATAL_ERROR "open-async ${tag}: open_prepare_ms_max is ${CMAKE_MATCH_1}, want < ${OPEN_ASYNC_BOUND_MS} (song open waited on the network on the UI thread)\n${o}")
  endif()
  set(${out} "${o}" PARENT_SCOPE)
endfunction()

function(need text pattern what)
  if(NOT text MATCHES "${pattern}")
    message(FATAL_ERROR "open-async: ${what}: no match for ${pattern}\n${text}")
  endif()
endfunction()

unset(ENV{SELECTA_PLUGIN_PATH}) # leg (a) plays with the built-in replayers; only leg (b) needs the probe plugin
run_leg(a "modland:Protracker/test/slowplay.mod" oa)
set(ENV{SELECTA_PLUGIN_PATH} "${PLUGINS}")
need("${oa}" "open_phases open_fetch_reads=[1-9]" "(a) the remote fetch ran on the song-start read slot")
need("${oa}" "songs_opened=1 " "(a) the song started once the file landed")
need("${oa}" "open_ui shown=[01] phase=[0-9]+ bar_draws=[0-9]+ btn_draws=[0-9]+ row_bars=[0-9]+ cancels=0" "(a) the open progress sentinels are dumped")
run_leg(b "${root}/probe.sfp" ob)
need("${ob}" "sysfile missing_reports=[0-9]+ fetches=1 installed=1 reopens=1 " "(b) the slow system file landed and the song re-opened")

# I1415m leg (c): the open is sent from the --input script, after the live loop is up, so the UI sees it in flight.
# bigslow.mod answers its body after 3 s. The previous song (slowplay.mod) plays; bigslow.mod is queued (QUEUE_ADD of a remote song) so its row is in the Songs list.
# Expect: load bar and Cancel button drawn, the row's own bar drawn, Esc cancels (cancels=1, nothing active), slowplay.mod keeps playing.
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "open-async leg c needs the sqlite3 command line tool")
endif()
unset(ENV{SELECTA_PLUGIN_PATH})
set(cdb "${root}/c.db")
set(ccfg "${root}/c-cfg")
file(MAKE_DIRECTORY "${ccfg}")
file(WRITE "${ccfg}/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_CACHE_DIR} "${root}/c-cache")
execute_process(COMMAND "${SELECTA}" --output null --size 80x25 --config-dir "${ccfg}" --db "${cdb}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats
  RESULT_VARIABLE rc ERROR_VARIABLE e OUTPUT_QUIET)
file(WRITE "${root}/c.sql" "INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(1, 'modland:Protracker/test/bigslow.mod', 'modland', 'Protracker/test/bigslow.mod', 'Big Slow', 'Nobody', 'Nowhere', 'mod', 1992, 'Chiptune', 120000, 1700000000);\n")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${cdb}" INPUT_FILE "${root}/c.sql" RESULT_VARIABLE rc ERROR_VARIABLE e)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "open-async c: inserting the row failed: ${e}")
endif()
file(WRITE "${root}/c-in.txt" "wait 100\nexec OPEN modland:Protracker/test/slowplay.mod\nexec PLAY\nsleep 3500\nexec OPEN modland:Protracker/test/bigslow2.mod\nsleep 1000\nkey escape\nsleep 300\nkey escape\nsleep 800\n")
set(ENV{SELECTA_SDL_TEST_TIMED} "q@7500")
execute_process(COMMAND "${SELECTA}" --output null --config-dir "${ccfg}" --db "${cdb}" --ui desktop --backend sdl
  --view songs --canvas 1280x800 --clock fixed --stats --exec "QUEUE_ADD modland:Protracker/test/bigslow.mod" --input "${root}/c-in.txt"
  OUTPUT_VARIABLE oc ERROR_VARIABLE ec RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "open-async c: selecta exited ${rc}\n${oc}${ec}")
endif()
need("${oc}" "open_ui shown=0 phase=[0-9]+ bar_draws=[1-9][0-9]* btn_draws=[1-9][0-9]* row_bars=[1-9][0-9]* cancels=1" "(c) bar, Cancel button and row bar drawn while in flight; Esc cancelled once; nothing active after")
need("${oc}" "songs_opened=1[^0-9]" "(c) only the first song opened; the cancelled open never started, the previous song keeps playing")
message(STATUS "open-async c: ok")

# I1415m: closing the app while a song download is in flight must not wait for the download (bigslow.mod body takes 3 s).
find_program(PYTHON3 python3)
file(WRITE "${root}/d-in.txt" "wait 100\nsleep 300\nexec OPEN modland:Protracker/test/bigslow2.mod\nsleep 600\n")
set(ENV{SELECTA_SDL_TEST_TIMED} "q@1000")
set(ENV{SELECTA_CACHE_DIR} "${root}/d-cache")
execute_process(COMMAND "${PYTHON3}" -I -c "import subprocess, sys, time\nt = time.time()\nr = subprocess.run(sys.argv[1:], capture_output=True, timeout=60)\nprint('exit_ms=%d rc=%d' % ((time.time() - t) * 1000, r.returncode))"
  "${SELECTA}" --output null --config-dir "${ccfg}" --db "${cdb}" --ui desktop --backend sdl --view home --canvas 1280x800 --clock fixed --input "${root}/d-in.txt"
  OUTPUT_VARIABLE od RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT "${od}" MATCHES "exit_ms=([0-9]+) rc=0")
  message(FATAL_ERROR "open-async d: no clean exit: ${od}")
endif()
if(CMAKE_MATCH_1 GREATER 2500)
  message(FATAL_ERROR "open-async d: exit took ${CMAKE_MATCH_1} ms with a 3 s download in flight (shutdown waited for the transfer)")
endif()
message(STATUS "open-async d: exit ${CMAKE_MATCH_1} ms")
message(STATUS "open_async_ok")
