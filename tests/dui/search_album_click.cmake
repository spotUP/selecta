# dui-search-album-click (I1408): a click on an Albums row of the Search results activates that album (Johan 2026-11-16:
# "i cant click into albums in search results"). Through the real shell and the scripted --input feeder. -DSELECTA -DWORK
# The fixture library holds the album Lotus Turbo Challenge. Typing `/lotus` Enter lists it under Albums; the dump line `searchartist`
# with a cursor is the registered Albums row. Sentinel: the click raises opens= from 0 (the row was hit and sent VIEW_ACTIVATE).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()

execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%'"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "rekeying the fixture failed: ${err}")
endif()

function(sa_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()


set(typed "wait 1500\nkey /\nkey l\nkey o\nkey t\nkey u\nkey s\nkey enter\nwait 3000\nexec VIEW_CURSOR +0\nwait 5\n")
sa_run(base "${typed}")
if(NOT "${base}" MATCHES "searchartist 0 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) name=Lotus Turbo Challenge[^\n]* opens=0\n")
  message(FATAL_ERROR "no searchartist row for the album Lotus Turbo Challenge in:\n${base}")
endif()
math(EXPR x "${CMAKE_MATCH_1} + 40")
math(EXPR y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

sa_run(clk "${typed}move ${x} ${y}\nclick ${x} ${y}\nwait 200\n")
if(NOT "${clk}" MATCHES "view_source=search " OR NOT "${clk}" MATCHES "searches=([0-9]+)" OR CMAKE_MATCH_1 LESS 2)
  message(FATAL_ERROR "a click on the Albums row must run the album search (VIEW_ACTIVATE route):\n${clk}")
endif()
