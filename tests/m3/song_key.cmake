# T97 song key (plan 2026-10-25-m3-library.md, row I211; design doc 9, 6a). One song, one key in every table.
#   -DCLI=<selecta-cli> -DSEL=<song_key.sel> -DSQLITE3=<sqlite3 shell> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod>
# Part 1 (reachability): `selecta-cli analyse` stores a loudness gain, then the engine script plays the song to its
# end and sets a sticker, all from the plain path selecta-test.mod. The keys in songs (loudness), history,
# and stickers must all be local:selecta-test.mod; the history row must join to a songs row with a gain.
# Part 2 (migration): a schema v4 database holding raw-path rows is opened; v5 rewrites them to local: keys, keeps
# prefixed rows on a collision, leaves URLs and prefixed keys alone, and a second open changes nothing.
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

set(db "${WORK}/k.db")
execute_process(COMMAND "${CLI}" analyse --db "${db}" selecta-test.mod WORKING_DIRECTORY "${CWD}"
  OUTPUT_VARIABLE a ERROR_VARIABLE ae RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "analyse failed rc=${rc}\n${a}${ae}")
endif()
execute_process(COMMAND "${CLI}" script "${SEL}" --db "${db}" WORKING_DIRECTORY "${CWD}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "script failed rc=${rc}\n${out}${err}")
endif()
# I211b: the sticker answer events carry the canonical key although the script sent the plain path.
string(REGEX MATCHALL "SRC_KEY=[^,}\n ]*" answer_keys "${out}")
list(LENGTH answer_keys n_answer_keys)
if(n_answer_keys LESS 2)
  message(FATAL_ERROR "expected sticker answers with SRC_KEY, got: ${out}")
endif()
foreach(k IN LISTS answer_keys)
  expect_eq("sticker answer key" "${k}" "SRC_KEY=local:selecta-test.mod")
endforeach()
sql("${db}" "SELECT key FROM songs WHERE loudness_gain IS NOT NULL;" k_loud)
sql("${db}" "SELECT DISTINCT key FROM history;" k_hist)
sql("${db}" "SELECT DISTINCT key FROM stickers WHERE name = 'mood';" k_stk)
expect_eq("loudness key" "${k_loud}" "local:selecta-test.mod")
expect_eq("history key" "${k_hist}" "local:selecta-test.mod")
expect_eq("sticker key" "${k_stk}" "local:selecta-test.mod")
sql("${db}" "SELECT COUNT(*) FROM history h JOIN songs s ON s.key = h.key WHERE s.loudness_gain IS NOT NULL;" joined)
expect_eq("history rows joined to the song with a gain" "${joined}" "1")

# Part 2: migration from v4.
set(mdb "${WORK}/m.db")
execute_process(COMMAND "${CLI}" analyse --db "${mdb}" selecta-test.mod WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE a)
string(CONCAT seed
  "PRAGMA user_version = 4; "
  "INSERT INTO history(key, played_at, skipped) VALUES('selecta-test.mod', 10, 0), ('local:selecta-test.mod', 11, 0), "
  "('modland:p/purple.xm', 12, 0), ('http://x.test/a.mp3', 13, 0); "
  "INSERT INTO liked(key, at) VALUES('selecta-test.mod', 1), ('local:selecta-test.mod', 2); "
  "INSERT INTO stickers(key, name, value) VALUES('selecta-test.mod', 'mood', 'raw'), "
  "('local:selecta-test.mod', 'mood', 'kept'), ('selecta-test.mod', 'tempo', 'fast'); "
  "INSERT INTO ratings(key, rating) VALUES('Work:Music/a.mod', 3);")
sql("${mdb}" "${seed}" ignore)
execute_process(COMMAND "${CLI}" userdata --db "${mdb}" OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "userdata (migration open) failed rc=${rc}\n${o}${e}")
endif()
sql("${mdb}" "PRAGMA user_version;" ver)
expect_eq("schema version" "${ver}" "5")
sql("${mdb}" "SELECT group_concat(key || '/' || played_at, ' ') FROM (SELECT * FROM history ORDER BY played_at);" h)
expect_eq("history" "${h}" "local:selecta-test.mod/10 local:selecta-test.mod/11 modland:p/purple.xm/12 http://x.test/a.mp3/13")
sql("${mdb}" "SELECT group_concat(key || '/' || at, ' ') FROM (SELECT * FROM liked ORDER BY at);" l)
expect_eq("liked collision keeps the prefixed row" "${l}" "local:selecta-test.mod/2")
sql("${mdb}" "SELECT group_concat(key || '.' || name || '=' || value, ' ') FROM (SELECT * FROM stickers ORDER BY name);" s)
expect_eq("stickers" "${s}" "local:selecta-test.mod.mood=kept local:selecta-test.mod.tempo=fast")
sql("${mdb}" "SELECT key FROM ratings;" r)
expect_eq("AmigaDOS path" "${r}" "local:Work:Music/a.mod")
sql("${mdb}" "SELECT COUNT(*) FROM history; SELECT COUNT(*) FROM stickers;" before)
execute_process(COMMAND "${CLI}" userdata --db "${mdb}" OUTPUT_VARIABLE o)
sql("${mdb}" "SELECT COUNT(*) FROM history; SELECT COUNT(*) FROM stickers;" after)
expect_eq("second open is idempotent" "${after}" "${before}")
sql("${mdb}" "SELECT COUNT(*) FROM stats_plays WHERE key = 'local:selecta-test.mod';" plays)
if(plays LESS 1)
  message(FATAL_ERROR "stats_plays does not see the migrated plays: ${plays}")
endif()
message("song_key_ok")
