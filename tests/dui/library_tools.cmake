# dui-library-tools (I611 open part): the Library page tools row (magnifier, filter field, Sort by) filters and orders the PAGE's own
# rows, the engine's composer list, through the COMPOSERS_VIEW command. -DSELECTA -DSONGS -DWORK
# Known library: Allister Brimble 2 songs, Chris Huelsbeck 3, Jogeir Liljedahl 1. The libpage dump line carries the page row count,
# the first row, and view_sends, the count of COMPOSERS_VIEW lines the desktop UI sent (the sentinel that the new path ran).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-library-tools needs the sqlite3 command line tool to load the fixture library")
endif()
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
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:4', 'jogeir liljedahl', 'Jogeir Liljedahl'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:5', 'allister brimble', 'Allister Brimble'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('fixture:6', 'allister brimble', 'Allister Brimble')")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; ${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the composers failed: ${err}")
endif()

set(common --start-dir "${SONGS}" --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats)

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', want '${want}'")
  endif()
endfunction()

function(page_parse tag out)
  if(NOT "${out}" MATCHES "libpage rows=([0-9]+) first=\\[([^]]*)\\] view_sends=([0-9]+)")
    message(FATAL_ERROR "no libpage line in:\n${out}")
  endif()
  set(${tag}_rows "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_first "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_sends "${CMAKE_MATCH_3}" PARENT_SCOPE)
endfunction()

function(rect_centre outvar out re)
  if(NOT "${out}" MATCHES "${re}")
    message(FATAL_ERROR "no rect for '${re}' in:\n${out}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${outvar} "${cx} ${cy}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(p0 ${common} --input "${WORK}/idle.txt")
page_parse(p0 "${p0}")
expect_eq("page rows before any tool" "${p0_rows}" "3")
expect_eq("default order is alphabetical" "${p0_first}" "Allister Brimble")
expect_eq("no COMPOSERS_VIEW before any tool use" "${p0_sends}" "0")
rect_centre(mag "${p0}" "libtools [^\n]* mag ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ")
rect_centre(sortc "${p0}" "libtools [^\n]* sort (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)")

# 1. Typing "ch" in the page field leaves Chris Huelsbeck only.
file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/f1.txt" "click ${mag}\nwait 100\nkey c\nwait 50\nkey h\nwait 100\n")
dui_run(f1 ${common} --input "${WORK}/f1.txt")
page_parse(f1 "${f1}")
expect_eq("filter ch leaves one page row" "${f1_rows}" "1")
expect_eq("filter ch keeps Chris Huelsbeck" "${f1_first}" "Chris Huelsbeck")
if(f1_sends LESS 1)
  message(FATAL_ERROR "the page filter never reached the engine (view_sends=${f1_sends})")
endif()

# 2. A filter that matches two names: "l" matches Allister, Chris Huelsbeck (l), Jogeir Liljedahl.
file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/f2.txt" "click ${mag}\nwait 100\nkey j\nwait 100\n")
dui_run(f2 ${common} --input "${WORK}/f2.txt")
page_parse(f2 "${f2}")
expect_eq("filter j leaves Jogeir Liljedahl" "${f2_first}" "Jogeir Liljedahl")
expect_eq("filter j leaves one row" "${f2_rows}" "1")

# 3. Sort by Recently added: the composer with the most songs (Chris Huelsbeck, 3) leads.
file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/s0.txt" "click ${sortc}\nwait 100\n")
dui_run(s0 ${common} --input "${WORK}/s0.txt")
rect_centre(pick_added "${s0}" "lmenu 2 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/s1.txt" "click ${sortc}\nwait 100\nclick ${pick_added}\nwait 100\n")
dui_run(s1 ${common} --input "${WORK}/s1.txt")
page_parse(s1 "${s1}")
expect_eq("all rows stay under a sort" "${s1_rows}" "3")
expect_eq("Recently added puts the biggest composer first" "${s1_first}" "Chris Huelsbeck")
if(s1_sends LESS 1)
  message(FATAL_ERROR "the page sort never reached the engine (view_sends=${s1_sends})")
endif()
