# I535 source song counts (design doc 9). The Sources page reads each source's song count on the UI thread; schema v8
# keeps that count in source_songs by triggers so the read is a key seek, not a scan of songs (2.2 s on 520 000 songs).
#   -DCLI=<selecta-cli> -DSEL=<sources.sel> -DSQLITE3=<sqlite3 shell> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod>
# Part 1: insert, delete and a source change keep source_songs equal to COUNT(*) GROUP BY source.
# Part 2 (migration): a v7 database without the table is opened; v8 fills it from the songs rows.
# Part 3 (reachability): the SOURCES command reports the stored count (a sentinel no scan of songs could produce).
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
function(sql db text result)
  execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${text}" OUTPUT_VARIABLE o ERROR_VARIABLE e
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sqlite3 failed: ${e}")
  endif()
  string(STRIP "${o}" o)
  set(${result} "${o}" PARENT_SCOPE)
endfunction()
function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', want '${want}'")
  endif()
endfunction()
set(stored "SELECT group_concat(source || '=' || n, ' ') FROM (SELECT * FROM source_songs ORDER BY source);")
set(counted "SELECT group_concat(source || '=' || n, ' ') FROM (SELECT source, COUNT(*) AS n FROM songs GROUP BY source ORDER BY source);")

set(db "${WORK}/s.db")
execute_process(COMMAND "${CLI}" analyse --db "${db}" selecta-test.mod WORKING_DIRECTORY "${CWD}"
  OUTPUT_VARIABLE a ERROR_VARIABLE ae RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "analyse failed rc=${rc}\n${a}${ae}")
endif()
sql("${db}" "PRAGMA user_version;" ver)
expect_eq("schema version" "${ver}" "9")

# Part 1.
sql("${db}" "INSERT INTO songs(key, source, path) VALUES('modland:a', 'modland', 'a'), ('modland:b', 'modland', 'b'),
  ('modland:c', 'modland', 'c'), ('sndh:d', 'sndh', 'd'), ('sndh:e', 'sndh', 'e'); ${stored}" s)
expect_eq("after insert" "${s}" "local=1 modland=3 sndh=2")
sql("${db}" "DELETE FROM songs WHERE key = 'modland:a'; UPDATE songs SET source = 'modland' WHERE key = 'sndh:d';
  UPDATE songs SET path = 'e2' WHERE key = 'sndh:e'; ${stored}" s)
expect_eq("after delete and source change" "${s}" "local=1 modland=3 sndh=1")
sql("${db}" "DELETE FROM songs WHERE source = 'sndh'; ${stored}" s)
expect_eq("last song of a source removes its row" "${s}" "local=1 modland=3")
sql("${db}" "${counted}" c)
expect_eq("stored equals counted" "${s}" "${c}")

# Part 2.
sql("${db}" "DROP TRIGGER songs_source_ai; DROP TRIGGER songs_source_ad; DROP TRIGGER songs_source_au;
  DROP TABLE opened; DROP TABLE source_songs; INSERT INTO songs(key, source, path) VALUES('sndh:f', 'sndh', 'f'); PRAGMA user_version = 7;" ignore)
execute_process(COMMAND "${CLI}" userdata --db "${db}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "userdata (migration open) failed rc=${rc}\n${o}${e}")
endif()
sql("${db}" "PRAGMA user_version;" ver)
expect_eq("migrated schema version" "${ver}" "9")
sql("${db}" "${stored}" s)
expect_eq("migration fills source_songs" "${s}" "local=1 modland=3 sndh=1")

# Part 3: a direct UPDATE of source_songs fires no trigger, so 4242 can only come from the stored count.
sql("${db}" "UPDATE source_songs SET n = 4242 WHERE source = 'modland';" ignore)
execute_process(COMMAND "${CLI}" script "${SEL}" --db "${db}" WORKING_DIRECTORY "${CWD}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "script failed rc=${rc}\n${out}${err}")
endif()
if(NOT out MATCHES "SRC_KEY=modland,[^}]*SRC_COUNT=4242")
  message(FATAL_ERROR "SOURCES does not read source_songs:\n${out}")
endif()
message(STATUS "source_songs_ok")
