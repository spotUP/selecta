# I774n R10 text-ui-songinfo-sections: a fixture bake for selecta-test.mod is read by the real selecta binary (--config-dir);
# the song info page shows the Credits section. Empty sections (Other Sources, Found In) have no heading (Johan A12).
#   -DFIXTURE -DSELECTA -DKEYS -DCWD (dir holding selecta-test.mod) -DWORK
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
file(COPY "${CWD}/selecta-test.mod" DESTINATION "${WORK}/songs")
execute_process(COMMAND "${FIXTURE}" "${WORK}/songs/selecta-test.mod" "${WORK}/cfgsrc"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT out MATCHES "fixture_ok")
  message(FATAL_ERROR "fixture failed ${rc}\n${out}\n${err}")
endif()
file(GLOB baked "${WORK}/cfgsrc/out/*")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(COPY ${baked} DESTINATION "${WORK}/cfg")
execute_process(
  COMMAND "${SELECTA}" --output null --size 100x30 --config-dir "${WORK}/cfg" --start-dir "${WORK}/songs" --db "${WORK}/s.db"
          --exec "OPEN selecta-test.mod" --keys "${KEYS}" --screen-dump "${WORK}/screen.txt"
  WORKING_DIRECTORY "${WORK}/songs" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
file(READ "${WORK}/screen.txt" screen)
foreach(want "Credits" "Format" "Protracker")
  if(NOT screen MATCHES "${want}")
    message(FATAL_ERROR "want '${want}' on the song info page:\n${screen}")
  endif()
endforeach()
foreach(absent "Other Sources" "Found In")
  if(screen MATCHES "${absent}")
    message(FATAL_ERROR "empty section '${absent}' must have no heading:\n${screen}")
  endif()
endforeach()
message(STATUS "songinfo_sections ok")
