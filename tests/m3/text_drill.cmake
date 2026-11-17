# I1442b text-ui-drill: the text UI source drill pages (":source <id>", Formats / Artists levels, "/" filter) over uimodel/drill.h and the
# engine's drill_build. A scan makes the schema, sqlite3 adds three hvsc artists, then two runs: Artists unfiltered lists all three, the
# same with "/Alpha" lists only the two Alpha artists (the find text rides in the drill key and filters before windowing, a04fe076).
#   -DSELECTA=<exe> -DSQLITE3=<sqlite3> -DSONGS=<dir with .mod files> -DKEYS_SEED=<keys> -DKEYS_A -DKEYS_B -DWORK=<dir>
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${SELECTA}" --output null --size 100x30 --start-dir "${WORK}" --db "${WORK}/seed.db"
                        --exec "LIBRARY_SCAN ${SONGS}" --keys "${KEYS_SEED}"
                OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seed scan exited ${rc}\n${o}\n${e}")
endif()
string(CONCAT seed "PRAGMA trusted_schema = 1; DROP TRIGGER IF EXISTS songs_ai; "
  "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES "
  "('hvsc:1', 'hvsc', '/h/1.mod', 'One', 'Alpha One', '', 'Protracker', 1), "
  "('hvsc:2', 'hvsc', '/h/2.mod', 'Two', 'Alpha Two', '', 'Protracker', 1), "
  "('hvsc:3', 'hvsc', '/h/3.mod', 'Three', 'Beta Three', '', 'Protracker', 1);")
execute_process(COMMAND "${SQLITE3}" "${WORK}/seed.db" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE e)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "sqlite3 failed: ${e}")
endif()

function(run tag keys)
  file(COPY_FILE "${WORK}/seed.db" "${WORK}/${tag}.db")
  execute_process(
    COMMAND "${SELECTA}" --output null --size 100x30 --start-dir "${WORK}" --db "${WORK}/${tag}.db"
            --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "run ${tag} exited ${rc}\n${out}\n${err}")
  endif()
  file(READ "${WORK}/${tag}.txt" scr)
  set(scr_${tag} "${scr}" PARENT_SCOPE)
endfunction()

function(expect tag text what want)
  string(FIND "${${text}}" "${what}" at)
  if((want AND at LESS 0) OR (NOT want AND NOT at LESS 0))
    message(FATAL_ERROR "${tag}: '${what}' ${want} expected in:\n${${text}}")
  endif()
endfunction()

run(a "${KEYS_A}")
expect(a scr_a "Alpha One" 1)
expect(a scr_a "Alpha Two" 1)
expect(a scr_a "Beta Three" 1)

run(b "${KEYS_B}")
expect(b scr_b "Alpha One" 1)
expect(b scr_b "Alpha Two" 1)
expect(b scr_b "Beta Three" 0)
expect(b scr_b "Filter: Alpha" 1)
message(STATUS "text_drill_ok")
