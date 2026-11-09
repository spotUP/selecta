# library-second-instance (I1162): a second Selecta opens the library database while another process holds a lock on it.
# Bug (Johan, Mac): the text UI started beside the running desktop app stopped with "cannot open the library database
# ~/.config/selecta/library.db". The library connection had no busy handler, so the open's first write got SQLITE_BUSY at once.
# Driven through the real selecta binary: a sqlite3 process takes a write lock (BEGIN IMMEDIATE plus a write) on the database
# and holds it for 2 s; Selecta starts 0.5 s into that and must
# 1. exit 0 and not print "cannot open the library database";
# 2. trace "library busy, waiting" (the sentinel: the busy handler ran, so the open waited for the lock instead of failing).
# -DSELECTA -DWORK
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "library-second-instance needs the sqlite3 command line tool to hold the lock")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_SOCKET} "${WORK}/second.sock")
set(db "${WORK}/lib.db")
set(args --output null --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend mem --view home --stats --db "${db}")
execute_process(COMMAND "${SELECTA}" ${args} RESULT_VARIABLE rc OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "creating the database failed (${rc}):\n${e}")
endif()
# The lock holder and Selecta must overlap in time, so both run from one shell script.
list(JOIN args "' '" quoted)
file(WRITE "${WORK}/run.sh" "
( printf \"BEGIN IMMEDIATE;\\nUPDATE meta SET value=value WHERE key='account';\\n\"; sleep 2; printf 'COMMIT;\\n' ) | '${SQLITE3}' '${db}' &
sleep 0.5
'${SELECTA}' --trace '${quoted}'
rc=\$?
wait
exit \$rc
")
execute_process(COMMAND sh "${WORK}/run.sh" RESULT_VARIABLE rc OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(NOT rc EQUAL 0 OR e MATCHES "cannot open the library database")
  message(FATAL_ERROR "Selecta did not start beside a process holding the database lock (exit ${rc}):\n${e}")
endif()
if(NOT e MATCHES "library busy, waiting")
  message(FATAL_ERROR "no 'library busy, waiting' trace: the lock was not held when Selecta opened the database, or the busy handler did not run")
endif()
message(STATUS "library-second-instance: waited for the lock and started")
