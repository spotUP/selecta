# T91 home-view (plan 2026-10-25-m3-library.md, I191).
#   -DCLI=<selecta-cli> -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DSEL=<history.sel> -DSQLITE3=<sqlite3 shell>
#   -DKEYS=<keys> -DGOLDEN=<screen golden> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod>
# history.sel writes a finished play and a skip through the engine. sqlite3 then seeds an album on the skipped song, a
# newer play and the modland "new this week" count a changelog sync stores. The real selecta binary opens that file and
# key H shows the home view. home_rows= must equal the sum counted from the database here with separate queries:
# distinct songs played + albums with an unfinished play + the new-this-week count.
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
string(CONCAT seed
  "UPDATE songs SET album = 'Hardcore Heroes' WHERE key = 'selecta-test-mk.mod'; "
  "INSERT INTO history(key, played_at, skipped) VALUES('selecta-test.mod', 4102444800, 0); "
  "INSERT INTO meta(key, value) VALUES('modland.new_this_week', '7') ON CONFLICT(key) DO UPDATE SET value = '7'")
sql("${seed}" ignore)
sql("SELECT COUNT(DISTINCT key) FROM history;" recent)
string(CONCAT jumpq
  "SELECT COUNT(DISTINCT s.album) FROM history h JOIN songs s ON s.key = h.key WHERE h.skipped = 1 AND s.album IS NOT NULL "
  "AND NOT EXISTS (SELECT 1 FROM history f WHERE f.key = h.key AND f.skipped = 0);")
sql("${jumpq}" jump)
sql("SELECT value FROM meta WHERE key = 'modland.new_this_week';" fresh)
math(EXPR want "${recent} + ${jump} + ${fresh}")
if(NOT recent EQUAL 2 OR NOT jump EQUAL 1 OR NOT fresh EQUAL 7)
  message(FATAL_ERROR "seed counts recent=${recent} jump=${jump} new=${fresh}, want 2 1 7")
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
if(NOT CMAKE_MATCH_1 EQUAL want)
  message(FATAL_ERROR "home_rows=${CMAKE_MATCH_1}, want ${want} counted from the database")
endif()
file(READ "${WORK}/home.txt" screen)
if(NOT EXISTS "${GOLDEN}")
  file(WRITE "${GOLDEN}" "${screen}")
  message(FATAL_ERROR "golden written; review it")
endif()
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "home_rows=${want}")
