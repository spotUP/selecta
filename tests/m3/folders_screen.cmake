# T83 text-ui-folders-tree (plan 2026-10-25-m3-library.md, I179).
#   -DCLI=<selecta-cli> -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DSEL=<folders.sel> -DKEYS=<keys>
#   -DGOLDEN=<screen golden> -DWORK=<dir> -DCWD=<dir holding selecta-test.mod and selecta-test-mk.mod, copied into ${WORK}/songs>
# The DB half (folders.sel) builds the folder tree in a database file; the real selecta binary then opens
# that file, key f shows the tree through the engine's folders view source, Down Down Enter plays Mix_one.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# Private song directory: the browser lists the directory, so sharing build/.../songs with sibling tests (which leave
# a/, b/, c/, album.stamp, sd_*.wav there) made the screen depend on test order.
set(songs "${WORK}/songs")
file(MAKE_DIRECTORY "${songs}")
foreach(m selecta-test.mod selecta-test-mk.mod)
  file(COPY "${CWD}/${m}" DESTINATION "${songs}")
endforeach()
set(CWD "${songs}")
execute_process(
  COMMAND "${CLI}" script "${SEL}" --catalogue "${CAT}" --db "${WORK}/f.db"
  WORKING_DIRECTORY "${CWD}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --db "${WORK}/f.db"
          --keys "${KEYS}" --screen-dump "${WORK}/f.txt"
  WORKING_DIRECTORY "${CWD}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
foreach(want "folders_screen opens=1 rows_built=" "view_source=folders" "songs_opened=1")
  string(FIND "${out}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "missing '${want}' in:\n${out}")
  endif()
endforeach()
file(READ "${WORK}/f.txt" screen)
if(NOT EXISTS "${GOLDEN}")
  file(WRITE "${GOLDEN}" "${screen}")
  message(FATAL_ERROR "golden written; review it")
endif()
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "folders_screen ok")
