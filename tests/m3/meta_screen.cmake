# T87 metadata-panel (plan 2026-10-25-m3-library.md, I185).
#   -DCLI=<selecta-cli> -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DSQLITE3=<sqlite3 shell> -DKEYS=<keys>
#   -DGOLDEN=<screen golden> -DGOLDEN_LOCAL=<screen golden> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod>
# The catalogue is imported into a database file; sqlite3 then seeds what a HVSC sync and the genre step would have
# written for selecta-test.mod (STIL sticker, composer, release, year, genre, rating, two finished plays). The real
# selecta binary opens that file, plays the song and key i shows the panel. selecta-test-mk.mod has no catalogue data
# (its year is removed): its panel must show replayer metadata only, with no catalogue rows at all.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(db "${WORK}/m.db")
execute_process(COMMAND "${CLI}" script "${SEL}" --catalogue "${CAT}" --db "${db}"
  WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
string(CONCAT seed_sql "PRAGMA trusted_schema = 1; "
  "UPDATE songs SET album = 'Hardcore Heroes', year = 1994, genre = 'Electronic - Techno' WHERE key = 'local:selecta-test.mod'; "
  "INSERT INTO stickers(key, name, value) VALUES('local:selecta-test.mod', 'stil.comment', 'Rob''s title tune for the Elite conversion'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('local:selecta-test.mod', 'lizardking', 'Lizardking'); "
  "INSERT INTO ratings(key, rating) VALUES('local:selecta-test.mod', 4); "
  "INSERT INTO history(key, played_at, skipped) VALUES('local:selecta-test.mod', 1, 0), ('local:selecta-test.mod', 2, 0), ('local:selecta-test.mod', 3, 1); "
  "UPDATE songs SET year = NULL WHERE key = 'local:selecta-test-mk.mod';")
execute_process(COMMAND "${SQLITE3}" "${db}" "${seed_sql}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "sqlite3 seed failed: ${err}")
endif()

function(run_panel song dump)
  execute_process(
    COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --db "${db}"
            --exec "OPEN ${song}" --keys "${KEYS}" --screen-dump "${WORK}/${dump}"
    WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  endif()
  set(panel_out "${out}" PARENT_SCOPE)
endfunction()

function(check_golden dump golden)
  file(READ "${WORK}/${dump}" screen)
  if(NOT EXISTS "${golden}")
    file(WRITE "${golden}" "${screen}")
    message(FATAL_ERROR "golden written; review it")
  endif()
  file(READ "${golden}" want)
  if(NOT screen STREQUAL want)
    message(FATAL_ERROR "screen differs from ${golden}\nexpected:\n${want}\ngot:\n${screen}")
  endif()
endfunction()

run_panel(selecta-test.mod full.txt)
if(NOT panel_out MATCHES "meta_panel panel_loads=1 meta_panel_rows=7")
  message(FATAL_ERROR "full song: want 'meta_panel panel_loads=1 meta_panel_rows=7' in:\n${panel_out}")
endif()
check_golden(full.txt "${GOLDEN}")

run_panel(selecta-test-mk.mod local.txt)
if(NOT panel_out MATCHES "meta_panel panel_loads=1 meta_panel_rows=0")
  message(FATAL_ERROR "local song: want 'meta_panel panel_loads=1 meta_panel_rows=0' in:\n${panel_out}")
endif()
check_golden(local.txt "${GOLDEN_LOCAL}")
message(STATUS "meta_screen ok")
