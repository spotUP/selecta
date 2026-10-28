# dui-topbar-doors (I615a): where the top bar owns Home and Search, the sidebar has no Home or Search row. -DSELECTA -DSONGS -DWORK
# 1. Default tier (1280x800, top bar shown): the sidebar_doors dump says only doors 2..6 (Your Library and below) got a row, doors 0
#    and 1 have empty rectangles, and a `door 0` input click does nothing (the page stays).
# 2. The top bar Home button still opens Home, and typing in the field still opens Search with results.
# 3. Bitmap tier (--tier bitmap or the Amiga canvas): no top bar, so all seven doors keep a row (bits 0..6 set).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-topbar-doors needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
file(READ "${HERE}/fixture_library.sql" fixture)
string(REGEX MATCHALL "INSERT INTO songs\\([^\n]*'mod', [0-9]+," mods "${fixture}")
list(LENGTH mods mod_count)

function(run tag view canvas extra script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view ${view} --canvas ${canvas} --clock fixed --stats ${extra} --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "sidebar_doors drawn=([0-9]+)")
    message(FATAL_ERROR "no sidebar_doors line (the I615a sentinel) in:\n${o}")
  endif()
  set(${tag}_drawn ${CMAKE_MATCH_1} PARENT_SCOPE)
  string(REGEX MATCH "topbar draws=([0-9]+) shown=([01])" _m "${o}")
  set(${tag}_shown ${CMAKE_MATCH_2} PARENT_SCOPE)
  foreach(n 0 1 2)
    string(REGEX MATCH "\ndoor ${n} (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)" _m "${o}")
    set(${tag}_door${n}w ${CMAKE_MATCH_3} PARENT_SCOPE)
  endforeach()
  string(REGEX MATCH "nav page=([a-z]+)" _m "${o}")
  set(${tag}_page "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_out "${o}" PARENT_SCOPE)
endfunction()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', want '${want}'")
  endif()
endfunction()

# 1. default tier: doors 2..6 drawn (bits 2-6 = 124), 0 and 1 not
run(def library 1280x800 "" "wait 100\n")
expect_eq("the top bar is shown" "${def_shown}" 1)
expect_eq("doors drawn on the default tier" "${def_drawn}" 124)
expect_eq("Home door has no rectangle" "${def_door0w}" 0)
expect_eq("Search door has no rectangle" "${def_door1w}" 0)
if(NOT def_door2w GREATER 0)
  message(FATAL_ERROR "Your Library must keep its sidebar row (door 2 width ${def_door2w})")
endif()
run(dead library 1280x800 "" "door 0\ndoor 1\nwait 100\n")
expect_eq("a click where Home used to be does nothing" "${dead_page}" "library")

# 2. the top bar still reaches Home and Search
run(home library 1280x800 "" "topbar 0\nwait 100\n")
expect_eq("top bar Home opens Home" "${home_page}" "home")
run(find home 1280x800 "" "topbar 1\nkey f\nkey o\nkey r\nkey m\nkey a\nkey t\nkey :\nkey m\nkey o\nkey d\nwait 600\n")
expect_eq("top bar field opens Search" "${find_page}" "search")
if(NOT "${find_out}" MATCHES "search draws [1-9][0-9]* rows ${mod_count} query format:mod\n")
  message(FATAL_ERROR "typing in the top bar field must give ${mod_count} mod results:\n${find_out}")
endif()

# 3. the 900x600 canvas below the top bar's height budget is still default tier; the bitmap tier has no bar: all doors
run(bmp home 640x256 "--ydiv 2 --zoom fit --resize 640x512" "wait 100\n")
expect_eq("no top bar on the bitmap tier" "${bmp_shown}" 0)
expect_eq("all seven doors on the bitmap tier" "${bmp_drawn}" 127)
