# I1440p2 text-ui-tiles-back: the real selecta binary; Back from the first drill page of a C64 SID tile lands on the tiles page, not Home.
#   -DSELECTA -DKEYS -DWORK
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${WORK}/songs" --db "${WORK}/s.db"
          --keys "${KEYS}" --screen-dump "${WORK}/screen.txt"
  WORKING_DIRECTORY "${WORK}/songs" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
file(READ "${WORK}/screen.txt" screen)
if(NOT screen MATCHES "Browse  C64 SID")
  message(FATAL_ERROR "tiles page missing after Back:\n${screen}")
endif()
message(STATUS "tiles_back ok")
