# dui-home-copper (I628): on an AGA canvas the Home page is one copper bar top to bottom and every generated cover is a
# copper gradient. -DSELECTA -DWORK
# Johan, AGA rig 2, Home: "the covers dont use copper colors now", "the home bg is not a copperbar either". The copper
# had 4 pens and gave each distinct gradient its own pen for the whole screen: the 300 lpx header band took one, the
# first three generated covers took the rest and every later cover fell back to the dither; the background below the
# band was plain panel. A copper pen is a per-scanline colour, so gradients on disjoint rows can share one pen and the
# covers of one shelf row share the row's key (the I510 decision). Rendered at 640x256 ydiv 2 INDEX8 with the copper on
# (--copper 4, what the aga backend enables), the "copper" stats lines must show:
#  1. the three generated covers of the shelf row have three different colour pairs, and a copper pen shows one colour per
#     scanline: the first cover rides the row's key, the other two dither in their own colours (miss=2). The quick-access
#     grid above the shelves (design section 11) is a second cover row with its own key, so keys = background + 2 rows. A cover
#     that reused the first one's key would show cover 1's colours (Johan's decision 1); miss=0 here means that bug;
#  2. a background key that spans the page: over 150 of the 256 rows, from the main region's top to its bottom;
#  3. each cover row (quick grid, shelf) holds exactly one cover key (no two keys on the same rows), all on one pen.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-home-copper needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; DELETE FROM artwork" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "dropping the covers failed: ${err}") # every tile draws generated art
endif()
set(execs "")
foreach(q 1.mod 2.mod 3.xm 4.it)
  list(APPEND execs --exec "QUEUE_ADD \"fixture/${q}\"")
endforeach()
dui_run(shot --db "${db}" --ui desktop --backend mem --view home --canvas 640x256 --ydiv 2 --format index8 --copper 4 --zoom fit
  --clock fixed --stats ${execs})
if(NOT "${shot}" MATCHES "\ncopper pens=4 keys=([0-9]+) miss=([0-9]+) changes=([0-9]+)\n")
  message(FATAL_ERROR "no copper stats line in:\n${shot}")
endif()
set(keys ${CMAKE_MATCH_1})
set(miss ${CMAKE_MATCH_2})
message(STATUS "copper keys=${keys} miss=${miss} changes=${CMAKE_MATCH_3}")
if(NOT miss EQUAL 2)
  message(FATAL_ERROR "expected 2 dithered covers on the shelf row (miss=2), got keys=${keys} miss=${miss}:\n${shot}")
endif()
string(REGEX MATCHALL "copper_key [0-9]+ -?[0-9]+ [0-9]+ -?[0-9]+ [0-9]+" lines "${shot}")
set(bg "")
set(rows "")
set(cover_pens "")
foreach(l IN LISTS lines)
  string(REGEX MATCH "copper_key ([0-9]+) (-?[0-9]+) ([0-9]+) (-?[0-9]+) ([0-9]+)" _ "${l}")
  math(EXPR span "${CMAKE_MATCH_5} - ${CMAKE_MATCH_4}")
  if(span GREATER 150 AND bg STREQUAL "")
    set(bg "${l}")
    math(EXPR end "${CMAKE_MATCH_2} + ${CMAKE_MATCH_3} - 2") # the main region clip may end up to 2 rows above the halved rect
    if(CMAKE_MATCH_4 GREATER 8 OR CMAKE_MATCH_5 LESS end)
      message(FATAL_ERROR "the Home background copper bar does not run from the page top to its bottom (${end}): ${l}")
    endif()
  else()
    list(APPEND rows "${CMAKE_MATCH_2}/${CMAKE_MATCH_3}")
    list(APPEND cover_pens "${CMAKE_MATCH_1}")
  endif()
endforeach()
if(bg STREQUAL "")
  message(FATAL_ERROR "no copper key spans the Home page (the background is not a copper bar):\n${shot}")
endif()
list(LENGTH rows nrows)
set(urows ${rows})
list(REMOVE_DUPLICATES urows)
list(LENGTH urows nurows)
list(REMOVE_DUPLICATES cover_pens)
list(LENGTH cover_pens npens)
string(REGEX MATCHALL "\nshelf [0-9]+ tile [0-9]+ " tiles "${shot}")
list(LENGTH tiles ntiles)
message(STATUS "background ${bg}; cover rows ${rows}; cover pens ${cover_pens}; shelf tiles ${ntiles}")
if(nrows LESS 2 OR ntiles LESS 3)
  message(FATAL_ERROR "the fixture drew ${ntiles} shelf tiles and ${nrows} cover keys: too few to test the copper covers:\n${shot}")
endif()
if(NOT nrows EQUAL nurows)
  message(FATAL_ERROR "covers on one row hold separate copper keys (${rows}): the row should share one line colour")
endif()
if(NOT npens EQUAL 1)
  message(FATAL_ERROR "the cover rows ride ${npens} pens (${cover_pens}): disjoint rows should share one")
endif()
math(EXPR want_keys "${nrows} + 1")
if(NOT keys EQUAL want_keys)
  message(FATAL_ERROR "expected the background key plus one key per cover row (keys=${want_keys}), got keys=${keys}:\n${shot}")
endif()
