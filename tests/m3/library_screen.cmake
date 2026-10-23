# T98 text-ui-library-screen (plan 2026-10-25-m3-library.md, I205/I206).
#   -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DKEYS=<keys> -DGOLDEN=<screen golden>
#   -DDBKEYS=<keys> -DSONGS=<dir with .mod files> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod>
# Part 1: --catalogue, keys L / lizardking Enter Enter Down Enter. The exit lines must show the
# Library screen opened once, two searches (all songs, then the query) and one song played, and
# the last screen must equal the golden.
# Part 2: --db on a temp file, filled by --exec LIBRARY_SCAN; L lists the scanned songs.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --catalogue "${CAT}"
          --keys "${KEYS}" --screen-dump "${WORK}/lib.txt"
  WORKING_DIRECTORY "${CWD}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
foreach(want "library_screen opens=1 searches=2" "view_source=query" "songs_opened=1")
  string(FIND "${out}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "missing '${want}' in:\n${out}")
  endif()
endforeach()
file(READ "${WORK}/lib.txt" screen)
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()

execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${WORK}" --db "${WORK}/library.db"
          --exec "LIBRARY_SCAN ${SONGS}" --keys "${DBKEYS}" --screen-dump "${WORK}/db.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "--db run exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
string(FIND "${out}" "library_screen opens=1 searches=1" at)
if(at LESS 0)
  message(FATAL_ERROR "--db run: library screen did not open\n${out}")
endif()
file(READ "${WORK}/db.txt" dbscreen)
if(NOT dbscreen MATCHES "Library by format  Tree\n \\[\\+\\] mod \\([1-9][0-9]*\\)")
  message(FATAL_ERROR "--db run: no scanned rows on the screen:\n${dbscreen}")
endif()
if(NOT EXISTS "${WORK}/library.db")
  message(FATAL_ERROR "--db run: no database file")
endif()
message(STATUS "library_screen ok")
