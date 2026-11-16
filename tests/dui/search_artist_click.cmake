# dui-search-artist-click (I1207): a click on an Artists row of the Search results opens that artist's collection page (Johan 2026-11-09:
# "clicking an artis in the search results does nothing"). Through the real shell and the scripted --input feeder. -DSELECTA -DWORK
# The fixture library holds the artist Chris Huelsbeck. Typing `/huelsbeck` Enter lists him under Artists; the dump line `searchartist`
# is the registered Artists row's rectangle. Sentinels: `nav page=` is
# collection, `collection kind=artist` names the page kind; hist/pos prove the page entered the back history.
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

function(sa_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

set(typed "wait 5\nkey /\nkey h\nkey u\nkey e\nkey l\nkey s\nkey b\nkey e\nkey c\nkey k\nkey enter\nwait 600\nexec VIEW_CURSOR +0\nwait 5\n")
sa_run(base "${typed}")
if(NOT "${base}" MATCHES "searchartist [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) name=Chris Huelsbeck opens=0\n")
  message(FATAL_ERROR "no searchartist row for Chris Huelsbeck in:\n${base}")
endif()
math(EXPR x "${CMAKE_MATCH_1} + 40")
math(EXPR y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

sa_run(clk "${typed}move ${x} ${y}\nclick ${x} ${y}\nwait 200\n")
if(NOT "${clk}" MATCHES "nav page=collection ")
  message(FATAL_ERROR "a click on the Artists row must open the collection page:\n${clk}")
endif()
if(NOT "${clk}" MATCHES "collection kind=artist ")
  message(FATAL_ERROR "the opened collection page is not the artist page:\n${clk}")
endif()
if(NOT "${clk}" MATCHES "hist=([0-9]+) pos=([0-9]+)" OR CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "the artist page has no back history:\n${clk}")
endif()

# I1411c: artist spellings that differ only in case are one Artists row (Johan 2026-11-16: "mortimer Twang" next to
# "Mortimer Twang"). A song by "chris huelsbeck" joins the Chris Huelsbeck row; the first spelling seen is the one listed.
execute_process(COMMAND "${SQLITE3}" "${db}" "INSERT INTO songs(id, key, source, path, name, artist, format, duration_ms, added) VALUES(900, 'fixture:900', 'local', 'fixture/900.mod', 'Lowercase Tune', 'chris huelsbeck', 'mod', 60000, 1700000100)"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the lowercase artist failed: ${err}")
endif()
sa_run(fold "${typed}")
string(REGEX MATCHALL "searchartist [^\n]*name=[Cc]hris [Hh]uelsbeck " rows "${fold}")
list(LENGTH rows n)
if(NOT n EQUAL 1)
  message(FATAL_ERROR "I1411c: want one Chris Huelsbeck Artists row, got ${n}:\n${fold}")
endif()
if(NOT "${fold}" MATCHES "Lowercase Tune")
  message(WARNING "I1411c: the lowercase song is not in the dump, the fold case is not exercised")
endif()
