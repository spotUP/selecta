# I1191 text-ui-songinfo-page: the real selecta binary plays selecta-test.mod; the key file opens the Song Information
# page, types the note "ab", then Save Note, Mark Wrong and Skip Song. The sentinel proves the page sent three actions;
# sqlite3 reads back the note and the flags the engine wrote. Then the Now Playing page draws (second sentinel).
#   -DCLI -DSELECTA -DCAT -DSEL -DSQLITE3 -DKEYS -DCWD (dir holding selecta-test.mod) -DWORK
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
file(COPY "${CWD}/selecta-test.mod" DESTINATION "${WORK}/songs")
set(songs "${WORK}/songs")
set(db "${WORK}/s.db")
execute_process(COMMAND "${CLI}" script "${SEL}" --catalogue "${CAT}" --db "${db}"
  WORKING_DIRECTORY "${songs}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${songs}" --db "${db}"
          --exec "OPEN selecta-test.mod" --keys "${KEYS}" --screen-dump "${WORK}/screen.txt"
  WORKING_DIRECTORY "${songs}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "text_pages songinfo_actions=3 nowplay_draws=[1-9]")
  message(FATAL_ERROR "want 'text_pages songinfo_actions=3 nowplay_draws>=1' in:\n${out}")
endif()
file(READ "${WORK}/screen.txt" screen)
if(NOT screen MATCHES "Now Playing" OR NOT screen MATCHES "Next in Queue")
  message(FATAL_ERROR "Now Playing page missing from the final screen:\n${screen}")
endif()
execute_process(COMMAND "${SQLITE3}" "${db}" "SELECT note FROM notes;" OUTPUT_VARIABLE note RESULT_VARIABLE rc)
string(STRIP "${note}" note)
if(NOT note STREQUAL "ab")
  message(FATAL_ERROR "want stored note 'ab', got '${note}'")
endif()
execute_process(COMMAND "${SQLITE3}" "${db}" "SELECT wrong || '/' || skip FROM flags;" OUTPUT_VARIABLE fl RESULT_VARIABLE rc)
string(STRIP "${fl}" fl)
if(NOT fl STREQUAL "1/1")
  message(FATAL_ERROR "want flags wrong/skip '1/1', got '${fl}'")
endif()
message(STATUS "songinfo_page ok")
