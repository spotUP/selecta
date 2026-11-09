# dui-hippo-lists-resync (I1234): the desktop Playlists view builds the HippoPlayer group even when the library's hippo sync
# time is recent, because the group is what that time stands for. Johan's library was synced on 2026-10-30, before I1058
# made the hippo sync write a playlists folder: sync.hippo was 10 days old with sync_stale_days=30, so the view-open sync was
# never due and the Playlists page listed no HippoPlayer playlists.
#   -DSELECTA -DWORK -DNETWORK=<net fixture work dir> -DSQLITE3=<sel_sqlite3_shell>
# 1. A first run with automatic syncs off (sync_stale_days=0) creates the library and syncs nothing: no HippoPlayer row.
# 2. sync.hippo is set to now and sync_stale_days to 30 (the time test alone says "not due"); the Playlists view opens
#    against the recorded hippoplayer.se fixture. PASS: a top-level folder row HippoPlayer, playlists stored in it, and
#    the sentinel lists_syncs=1 (the view-open queued exactly one sync). Before the fix: lists_syncs=0, no HippoPlayer row.
# 3. A second open with the group in place queues no sync (lists_syncs=0): the folder check does not resync every open.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-hippo-lists-resync: -DSQLITE3=<sel_sqlite3_shell> missing")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "hippoplayer.se=${fx},fixture.test=${fx}")
set(ENV{SELECTA_HIPPO_SITE} "http://hippoplayer.se/") # the fixture speaks plain http
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
set(db "${WORK}/library.db")

function(hl_run outvar input)
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view playlists --canvas 1280x800 --clock fixed --stats
    --input "${WORK}/in.txt")
  if(NOT "${o}" MATCHES "folders_screen opens=[0-9]+ rows_built=[0-9]+ lists_syncs=([0-9]+)")
    message(FATAL_ERROR "no folders_screen lists_syncs line in:\n${o}")
  endif()
  set(${outvar}_syncs "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${outvar} "${o}\nstderr:\n${o_err}" PARENT_SCOPE)
endfunction()

set(waits "")
# `sleep` waits real time (the sync runs on a worker against the fixture); `wait` lets the fixed clock pump the answer in.
foreach(i RANGE 1 15)
  string(APPEND waits "sleep 200\nwait 100\n")
endforeach()

# 1. Library created, automatic syncs off: nothing synced.
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
hl_run(first "wait 300\n")
if(NOT "${first_syncs}" STREQUAL "0" OR "${first}" MATCHES "plrow [0-9]+ folder [0-9]+ 0 HippoPlayer")
  message(FATAL_ERROR "pass 1 must sync nothing (lists_syncs=${first_syncs}):\n${first}")
endif()

# 2. A recent sync time and no group: Johan's library after the I1058 upgrade.
string(TIMESTAMP now "%s" UTC)
file(WRITE "${WORK}/meta.sql" "INSERT OR REPLACE INTO meta(key, value) VALUES('sync.hippo', '${now}');\n")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${WORK}/meta.sql" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding sync.hippo failed (${rc}): ${err}")
endif()
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=30\n")
hl_run(second "${waits}")
if(NOT "${second_syncs}" STREQUAL "1")
  message(FATAL_ERROR "I1234: the Playlists view must queue one hippo sync when the HippoPlayer group is missing, lists_syncs=${second_syncs}:\n${second}")
endif()
if(NOT "${second}" MATCHES "plrow [0-9]+ folder [0-9]+ 0 HippoPlayer\n")
  message(FATAL_ERROR "I1234: no top-level HippoPlayer folder row on the Playlists page:\n${second}")
endif()
execute_process(COMMAND "${SQLITE3}" "${db}"
  "SELECT count(*) FROM playlists p JOIN folders f ON p.folder_id = f.id WHERE f.name = 'HippoPlayer' AND f.parent_id IS NULL;"
  OUTPUT_VARIABLE lists OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT "${lists}" GREATER 0)
  message(FATAL_ERROR "I1234: the HippoPlayer folder holds no playlists (count '${lists}', rc ${rc})")
endif()

# 3. Group in place, sync time recent: no sync on the next open.
hl_run(third "wait 300\n")
if(NOT "${third_syncs}" STREQUAL "0")
  message(FATAL_ERROR "I1234: an open with the HippoPlayer group present must not resync, lists_syncs=${third_syncs}:\n${third}")
endif()
