# dui-browse-modland-tile (I906b/I905): on the Browse page the Modland (source) tile opens the Modland source page and sends no
# SOURCE_SEARCH (Johan 2026-11-01; it used to open a Modules.pl search through the search scope). -DSELECTA -DWORK
# Reachability: Home at 1280x800, click Browse (the real top bar part), click the source tile labelled Modland. The dump's
# browse_source_opens=1 is the sentinel that the source branch of browse_click ran; on_searches=0 says no SOURCE_SEARCH went out;
# `nav page=collection` and `collection kind=source` say the source page is the one open.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-browse-modland-tile needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
set(common --db "${db}" --ui desktop --backend mem --view home --canvas 1280x1800 --clock fixed --stats)
dui_run(init ${common})
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES "
  "('modland:m1.mod','modland','m1.mod','Mod one','Ann','','Protracker',500),"
  "('modland:m2.xm','modland','m2.xm','Mod two','Bob','','Fasttracker 2',501)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the modland source failed: ${err}")
endif()

# 1. the tiles page holds a source tile for Modland
file(WRITE "${WORK}/tiles.txt" "wait 100\ntopbar 2\nwait 600\n")
dui_run(t ${common} --input "${WORK}/tiles.txt")
if(NOT "${t}" MATCHES "browse tile ([0-9]+) 2 [Mm]odland[^\n]*\n")
  message(FATAL_ERROR "the Browse page must hold a source tile labelled Modland:\n${t}")
endif()
set(tile ${CMAKE_MATCH_1})
if(NOT "${t}" MATCHES "browse_source_opens=0 on_searches=0\n")
  message(FATAL_ERROR "before the click no source tile was opened and no SOURCE_SEARCH sent:\n${t}")
endif()

# 2. the Modland tile opens the Modland source page, no SOURCE_SEARCH
file(WRITE "${WORK}/open.txt" "wait 100\ntopbar 2\nwait 600\nbrowse ${tile}\nwait 800\n")
dui_run(o ${common} --input "${WORK}/open.txt")
if(NOT "${o}" MATCHES "browse_source_opens=1 on_searches=0\n")
  message(FATAL_ERROR "the Modland tile must take the source branch (browse_source_opens=1) and send no SOURCE_SEARCH (on_searches=0):\n${o}")
endif()
if(NOT "${o}" MATCHES "nav page=collection ")
  message(FATAL_ERROR "the Modland tile must open the collection page:\n${o}")
endif()
if(NOT "${o}" MATCHES "collection kind=source ")
  message(FATAL_ERROR "the collection page opened by the Modland tile must be the source page:\n${o}")
endif()
if("${o}" MATCHES "nav page=search ")
  message(FATAL_ERROR "the Modland tile must not open the Search page:\n${o}")
endif()
