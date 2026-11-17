# I774s text-ui-songinfo-scroll-*: the real selecta binary reads a fixture bake WITH other sources and provenance (Found In), runs
# a key file on the song info page and dumps the screen. The lines named in WANT must be on the final screen and the ones in
# ABSENT must not (lists, ';' separated).
#   -DFIXTURE -DSELECTA -DKEYS -DCWD (dir holding selecta-test.mod) -DWORK -DSIZE (80x25) -DWANT -DABSENT
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
file(COPY "${CWD}/selecta-test.mod" DESTINATION "${WORK}/songs")
execute_process(COMMAND "${FIXTURE}" "${WORK}/songs/selecta-test.mod" "${WORK}/cfgsrc" full
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT out MATCHES "fixture_ok")
  message(FATAL_ERROR "fixture failed ${rc}\n${out}\n${err}")
endif()
file(GLOB baked "${WORK}/cfgsrc/out/*")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(COPY ${baked} DESTINATION "${WORK}/cfg")
execute_process(
  COMMAND "${SELECTA}" --output null --size ${SIZE} --config-dir "${WORK}/cfg" --start-dir "${WORK}/songs" --db "${WORK}/s.db"
          --exec "OPEN selecta-test.mod" --keys "${KEYS}" --screen-dump "${WORK}/screen.txt"
  WORKING_DIRECTORY "${WORK}/songs" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
file(READ "${WORK}/screen.txt" screen)
foreach(want IN LISTS WANT)
  string(FIND "${screen}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "want '${want}' on the screen:\n${screen}")
  endif()
endforeach()
foreach(absent IN LISTS ABSENT)
  string(FIND "${screen}" "${absent}" at)
  if(NOT at LESS 0)
    message(FATAL_ERROR "'${absent}' must not be on the screen:\n${screen}")
  endif()
endforeach()
message(STATUS "songinfo_scroll ok")
