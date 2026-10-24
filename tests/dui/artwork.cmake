# dui-artwork (DUI-P2-4): the artwork worker over the fixture library. -DSELECTA -DSONGS -DWORK
# Albums 1 to 3 carry a cover, album 4 has none (tests/dui/fixture_library.sql). The home view queues all four.
# 1. Run 1 decodes the three covers (STAT_ART_DECODES = art_decodes) and writes a thumb144 row for each.
# 2. Run 2 over the same database decodes nothing: the tiles come from the thumb rows.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-artwork needs the sqlite3 command line tool to load the fixture library")
endif()

# An empty run with --db creates the schema; the fixture is then loaded through sqlite3.
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 640x400 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()

file(STRINGS "${HERE}/fixture_library.sql" album_lines REGEX "^INSERT INTO albums")
set(execs "")
foreach(line IN LISTS album_lines)
  string(FIND "${line}" "VALUES(" at)
  string(SUBSTRING "${line}" ${at} -1 rest)
  string(FIND "${rest}" "'" q1)
  math(EXPR from "${q1} + 1")
  string(SUBSTRING "${rest}" ${from} -1 rest)
  string(FIND "${rest}" "'" q2)
  string(SUBSTRING "${rest}" 0 ${q2} title)
  list(APPEND execs --exec "QUEUE_ADD \"fixture/${title}.mod\"")
endforeach()
set(common --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats ${execs})

function(thumb_rows outvar)
  execute_process(COMMAND "${SQLITE3}" "${db}" "SELECT count(*) FROM artwork WHERE kind = 'thumb144'"
    OUTPUT_VARIABLE n RESULT_VARIABLE rc)
  string(STRIP "${n}" n)
  set(${outvar} "${n}" PARENT_SCOPE)
endfunction()

thumb_rows(before)
if(NOT before EQUAL 0)
  message(FATAL_ERROR "fixture must start without thumb rows, found ${before}")
endif()

dui_run(first ${common})
dui_stat(d1 "${first}" desktop art_decodes)
thumb_rows(after1)
if(NOT d1 EQUAL 3)
  message(FATAL_ERROR "run 1 must decode the 3 covers, art_decodes=${d1}")
endif()
if(NOT after1 EQUAL 3)
  message(FATAL_ERROR "run 1 must write 3 thumb144 rows, found ${after1}")
endif()

dui_run(second ${common})
dui_stat(d2 "${second}" desktop art_decodes)
thumb_rows(after2)
if(NOT d2 EQUAL 0)
  message(FATAL_ERROR "run 2 must decode nothing (thumbs are cached), art_decodes=${d2}")
endif()
if(NOT after2 EQUAL 3)
  message(FATAL_ERROR "run 2 must keep the 3 thumb rows, found ${after2}")
endif()
