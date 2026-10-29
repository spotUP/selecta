# open-fail-toast (I695b): a failed open raises the I652 toast whichever way the play started, with the engine's reason.
# -DSELECTA -DWORK -DFIXDIR -DNETWORK (the net-fixture work dir holding `port`) -DHERE
# The library holds one catalogue row (modland:Protracker/nodir/sub/ghost.mod) the fixture mirror answers 404 for. The row is queued;
# Down and Enter on it play it (not a drop: drop_playing stays 0). The open fails with http_status=404; the
# desktop dump line `overlay ... open_fail_toasts=N` is the sentinel that the new path ran, and the toast text is the
# engine's detail in plain words.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "fast.mirror.test=http://127.0.0.1:${port}")
set(ENV{SELECTA_MIRRORS_MODLAND} "http://fast.mirror.test/mods/")
# I695e: a cached SNDH file (no replayer reads SNDH yet) needs no network: the open reads it from the cache and finds no player.
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
file(MAKE_DIRECTORY "${WORK}/cache/files/sndh/Test")
file(WRITE "${WORK}/cache/files/sndh/Test/ghost.sndh" "SNDH not a real tune, only bytes no replayer probes for\n")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "open-fail-toast needs the sqlite3 command line tool")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
file(WRITE "${WORK}/rows.sql" "INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(1, 'modland:Protracker/nodir/sub/ghost.mod', 'modland', 'Protracker/nodir/sub/ghost.mod', 'Ghost Song', 'Nobody', 'Nowhere', 'mod', 1992, 'Chiptune', 120000, 1700000000);\n"
  "INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(2, 'sndh:Test/ghost.sndh', 'sndh', 'Test/ghost.sndh', 'Ghost Tune', 'Nobody', 'Nowhere', 'sndh', 1992, 'Chiptune', 120000, 1700000000);\n")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${WORK}/rows.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "inserting the row failed: ${err}")
endif()

function(play_run tag key script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view songs --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${key}" --input "${WORK}/${tag}.txt")
  set(${tag}_out "${o}" PARENT_SCOPE)
endfunction()

# Before any play: no failed-open toast.
play_run(idle modland:Protracker/nodir/sub/ghost.mod "wait 1\n")
if(NOT idle_out MATCHES "open_fail_toasts=0")
  message(FATAL_ERROR "sentinel not zero before the play:\n${idle_out}")
endif()

# The queued catalogue row plays with Down, Enter (a list play, not a drop: drop_playing stays 0).
play_run(played modland:Protracker/nodir/sub/ghost.mod "wait 1\nkey down\nkey enter\nwait 1000\n")
if(NOT played_out MATCHES "open_fail_toasts=1 ")
  message(FATAL_ERROR "the failed play from the queue did not raise exactly one toast (the engine emitted the open error twice before I695e):\n${played_out}")
endif()
if(NOT played_out MATCHES "open_fail_text=\"That song could not be downloaded \\(HTTP 404\\)\"")
  message(FATAL_ERROR "the toast text is not the engine's reason:\n${played_out}")
endif()

# I695e: a song with no replayer toasts once, naming the format from the key prefix.
play_run(nop sndh:Test/ghost.sndh "wait 1\nkey down\nkey enter\nwait 1000\n")
if(NOT nop_out MATCHES "open_fail_toasts=1 open_fail_text=\"No player for SNDH yet\"")
  message(FATAL_ERROR "the no_replayer play did not toast once with the format name:\n${nop_out}")
endif()
