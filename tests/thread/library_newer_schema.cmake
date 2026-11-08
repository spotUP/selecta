# library-newer-schema (I1162c): a Selecta older than the library database names the reason it cannot open it.
# Bug (Johan, Mac): build/sdl/apps/selecta/selecta (a stale static executable) beside the newer desktop app printed only
# "cannot open the library database <path>" for the real library.db, which the newer app had migrated to a schema version the
# stale executable does not know. The refusal had no message of its own, so it read like the lock failure of I1162.
# Fixture: a database Selecta made, then user_version raised past what any build supports. Driven through the real selecta binary:
# 1. exit 1 and the start-up message "cannot open the library database" (the refusal is still a refusal);
# 2. the reason on stderr: "library schema v999 is newer than this build" (the sentinel: the refusal path ran and spoke).
# -DSELECTA -DWORK
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "library-newer-schema needs the sqlite3 command line tool to raise user_version")
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
set(ENV{SELECTA_SOCKET} "${WORK}/newer.sock")
set(db "${WORK}/lib.db")
set(args --output null --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend mem --view home --stats --db "${db}")
execute_process(COMMAND "${SELECTA}" ${args} RESULT_VARIABLE rc OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "creating the database failed (${rc}):\n${e}")
endif()
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA user_version=999;" RESULT_VARIABLE rc ERROR_VARIABLE e)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "raising user_version failed (${rc}):\n${e}")
endif()
execute_process(COMMAND "${SELECTA}" ${args} RESULT_VARIABLE rc OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(rc EQUAL 0 OR NOT e MATCHES "cannot open the library database")
  message(FATAL_ERROR "a database newer than the build must be refused with the start-up error (exit ${rc}):\n${e}")
endif()
if(NOT e MATCHES "library schema v999 is newer than this build")
  message(FATAL_ERROR "the refusal gave no reason:\n${e}")
endif()
message(STATUS "library-newer-schema: refused with a reason")
