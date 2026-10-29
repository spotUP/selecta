# dui-browse-tiles (I615c): the top bar Browse icon opens the Browse tiles page. -DSELECTA -DSONGS -DWORK
# Reachability: Home at 1280x800, click Browse (the real top bar part), the page is "browse" and its draw counter runs with
# tiles built from the catalog (a Formats tile labelled mod); click that tile, the Search page opens with the filter
# format:mod drawn by the tile grid (browse filter=1, search view grid > 0) and holding only the fixture's mod songs.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-browse-tiles needs the sqlite3 command line tool to load the fixture library")
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

# 1. the tiles page
file(WRITE "${WORK}/tiles.txt" "wait 100\ntopbar 2\nwait 600\n")
dui_run(t --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --input "${WORK}/tiles.txt")
if(NOT "${t}" MATCHES "nav page=browse ")
  message(FATAL_ERROR "a click on Browse must open the Browse page:\n${t}")
endif()
if(NOT "${t}" MATCHES "browse draws=([0-9]+) tiles=([0-9]+) opens=0 filter=0\n" OR CMAKE_MATCH_1 LESS 1 OR CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "the Browse page must draw tiles (draws and tiles above 0):\n${t}")
endif()
if(NOT "${t}" MATCHES "browse tile ([0-9]+) 0 [Mm][Oo][Dd]\n")
  message(FATAL_ERROR "the Browse page must hold a Formats tile for mod:\n${t}")
endif()
set(mod_tile ${CMAKE_MATCH_1})

# 2. the mod tile opens a grid holding only mod songs
file(WRITE "${WORK}/open.txt" "wait 100\ntopbar 2\nwait 600\nbrowse ${mod_tile}\nwait 800\n")
dui_run(o --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --input "${WORK}/open.txt")
if(NOT "${o}" MATCHES "browse draws=[0-9]+ tiles=[0-9]+ opens=1 filter=1\n")
  message(FATAL_ERROR "a click on the mod tile must open its filter (opens=1 filter=1):\n${o}")
endif()
if(NOT "${o}" MATCHES "nav page=search ")
  message(FATAL_ERROR "the mod tile must open the Search page:\n${o}")
endif()
if(NOT "${o}" MATCHES "search draws ([0-9]+) rows ${mod_count} query format:mod\n")
  message(FATAL_ERROR "the mod tile must list exactly the ${mod_count} mod songs:\n${o}")
endif()
if(NOT "${o}" MATCHES "search view grid=([0-9]+) flow=0\n" OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "the filtered page must be drawn by the tile grid:\n${o}")
endif()

# 3. I711: the Browse page scrolls with the wheel. At 1280x300 the tile grid is taller than the panel: before the wheel the last tile
# is not drawn (lastvisible=0) and wheels=0; after a wheel past the end (the `browse scroll` sentinel counts the notches the
# Browse scroll path took) the offset is the maximum and the last tile is drawn (lastvisible=1).
file(WRITE "${WORK}/short.txt" "wait 100\ntopbar 2\nwait 600\n")
dui_run(s0 --db "${db}" --ui desktop --backend mem --view home --canvas 1280x300 --clock fixed --stats --input "${WORK}/short.txt")
if(NOT "${s0}" MATCHES "browse scroll y=0 max=([0-9]+) wheels=0 lastvisible=0\n" OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "I711: at 1280x300 the Browse tiles must overflow the panel with the last tile out of view:\n${s0}")
endif()
file(WRITE "${WORK}/wheel.txt" "wait 100\ntopbar 2\nwait 600\nmove 600 150\nwheel 0 -200\nwait 200\n")
dui_run(s1 --db "${db}" --ui desktop --backend mem --view home --canvas 1280x300 --clock fixed --stats --input "${WORK}/wheel.txt")
if(NOT "${s1}" MATCHES "browse scroll y=([0-9]+) max=([0-9]+) wheels=([0-9]+) lastvisible=1\n" OR CMAKE_MATCH_3 LESS 1 OR NOT CMAKE_MATCH_1 EQUAL CMAKE_MATCH_2 OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "I711: a wheel on the Browse page must scroll to the end and draw the last tile:\n${s1}")
endif()
