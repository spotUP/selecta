# dui-library (DUI-P4-3 slice A): the Library view with a composer drilldown, and the Stats page (F24). -DSELECTA -DSONGS -DWORK
# 1. --view library lists every composer with its song count (ui_library dump, from the engine's COMPOSERS_SHOW).
# 2. A click on a composer row drills in: the drill line carries the composer's song count from the library (3).
# 3. The Statistics totals in the dump equal `selecta-cli script` @stats_show over the same database.
# 4. A click on the back line leaves the drilldown.
# Slices B and C (tree, column browser, sources, Files, History) are not covered here.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-library needs the sqlite3 command line tool to load the fixture library")
endif()
get_filename_component(BINDIR "${SELECTA}" DIRECTORY)
set(CLI "${BINDIR}/../selecta-cli/selecta-cli")

set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:1', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:2', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:3', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:4', 'jogeir liljedahl', 'Jogeir Liljedahl')")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the composers failed: ${err}")
endif()

function(lib_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 900x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

lib_run(list "wait 1\n")
foreach(want "composer 0 chris huelsbeck|Chris Huelsbeck|3" "composer 1 jogeir liljedahl|Jogeir Liljedahl|1")
  string(FIND "${list}" "${want}" at)
  if(at LESS 0)
    message(FATAL_ERROR "library list lacks '${want}' in:\n${list}")
  endif()
endforeach()
if("${list}" MATCHES "drill ")
  message(FATAL_ERROR "drilldown shown before any click:\n${list}")
endif()

# The first composer row sits at the main panel's left margin below the title.
lib_run(drilled "wait 1\nmove 320 120\nclick 320 120\nwait 1\n")
if(NOT "${drilled}" MATCHES "drill Chris Huelsbeck\\|3\n")
  message(FATAL_ERROR "click on the first composer row did not drill in to 3 songs:\n${drilled}")
endif()

# Stats totals equal the CLI's.
file(WRITE "${WORK}/stats.sel" "@stats_show\n")
execute_process(COMMAND "${CLI}" script "${WORK}/stats.sel" --db "${db}" OUTPUT_VARIABLE cli RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli failed: ${err}")
endif()
string(REGEX MATCHALL "STAT [^\n]*" cli_rows "${cli}")
list(LENGTH cli_rows n)
if(n EQUAL 0)
  message(FATAL_ERROR "selecta-cli printed no stats:\n${cli}")
endif()
foreach(r IN LISTS cli_rows)
  string(REGEX REPLACE "^STAT ([^=]*)=(.*)$" "stat \\1|\\2" want "${r}")
  string(FIND "${list}" "${want}\n" at)
  if(at LESS 0)
    message(FATAL_ERROR "desktop stats lack '${want}' (from selecta-cli) in:\n${list}")
  endif()
endforeach()

# Back to the list.
lib_run(back "wait 1\nmove 320 120\nclick 320 120\nwait 1\nmove 320 108\nclick 320 108\nwait 1\n")
if("${back}" MATCHES "drill ")
  message(FATAL_ERROR "back line did not leave the drilldown:\n${back}")
endif()
