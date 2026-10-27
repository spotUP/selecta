# Home lists no history rows of songs that left the library (I555 History empty state).
#   -DCLI=<selecta-cli> -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DSEL=<history.sel> -DSQLITE3=<sqlite3 shell>
#   -DKEYS=<keys> -DCWD=<dir holding selecta-test.mod> -DWORK=<dir>
# history.sel writes a finished play and a skip through the engine. sqlite3 then empties the songs table, as a cleared
# library does, and leaves history alone. The real selecta binary opens that file and key H shows the home view:
# home_rows must be 0. A LEFT JOIN from history to songs lists the leftover keys as raw-path rows instead.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(db "${WORK}/h.db")
execute_process(COMMAND "${CLI}" script "${SEL}" --catalogue "${CAT}" --db "${db}"
  WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
function(sql text result)
  execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${text}" OUTPUT_VARIABLE o ERROR_VARIABLE e
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sqlite3 failed: ${e}")
  endif()
  string(STRIP "${o}" o)
  set(${result} "${o}" PARENT_SCOPE)
endfunction()
sql("SELECT COUNT(DISTINCT key) FROM history;" before)
sql("DELETE FROM songs; SELECT COUNT(*) FROM songs;" songs_left)
sql("SELECT COUNT(DISTINCT key) FROM history;" stale)
if(before LESS 1 OR NOT stale EQUAL before OR NOT songs_left EQUAL 0)
  message(FATAL_ERROR "seed: history keys ${before} -> ${stale}, songs left ${songs_left}; want history kept, songs 0")
endif()
execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --db "${db}"
          --keys "${KEYS}" --screen-dump "${WORK}/home.txt"
  WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "home_rows=([0-9]+)")
  message(FATAL_ERROR "no home_rows= sentinel in:\n${out}")
endif()
if(NOT CMAKE_MATCH_1 EQUAL 0)
  message(FATAL_ERROR "home_rows=${CMAKE_MATCH_1} with ${stale} stale history keys and no songs, want 0")
endif()
message(STATUS "home_stale_history_ok")
