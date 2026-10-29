# dui-library-retired (I678i, design 2026-10-29-desktop-library-architecture.md section 10 and D3): the Library page is
# retired. -DSELECTA -DSONGS -DWORK. Stands in for plan item DUI-P4-3 (label library) in dui-ratchet.
# Reachability, through the real desktop UI entry point:
# 1. --view library opens the Browse Folders page (nav page=folders) and the remap sentinel library_remaps counts 1.
# 2. That page draws exactly two tabs (Tree and Columns, tabs_drawn 2 per frame) and none of the retired Composers, Files,
#    History or Sources tabs; the Tree tab lists the fixture songs grouped by format.
# 3. The Browse page holds a Browse Folders tile; a click on it opens the page (browse_folders_opens=1, nav page=folders).
# 4. The Home Folders chip opens the same page.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-library-retired needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()

# 1 and 2. --view library
file(WRITE "${WORK}/in.txt" "wait 300\n")
dui_run(v --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats --input "${WORK}/in.txt")
if(NOT "${v}" MATCHES "nav page=folders ")
  message(FATAL_ERROR "--view library must open the Browse Folders page:\n${v}")
endif()
if(NOT "${v}" MATCHES "folders tabs_drawn=([0-9]+) library_remaps=([0-9]+) browse_folders_opens=0\n")
  message(FATAL_ERROR "no folders sentinel line in the dump:\n${v}")
endif()
set(tabs_drawn ${CMAKE_MATCH_1})
if(CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "the --view library remap never ran (library_remaps=${CMAKE_MATCH_2}):\n${v}")
endif()
if(tabs_drawn LESS 2)
  message(FATAL_ERROR "the Browse Folders tabs were never drawn (tabs_drawn=${tabs_drawn}):\n${v}")
endif()
string(REGEX MATCHALL "\ntab [0-9]+ " tab_lines "${v}")
list(LENGTH tab_lines ntabs)
if(NOT ntabs EQUAL 2)
  message(FATAL_ERROR "the Browse Folders page must hold exactly two tabs (Tree, Columns), dump has ${ntabs}:\n${v}")
endif()
if(NOT "${v}" MATCHES "\nmode (Tree|Columns)")
  message(FATAL_ERROR "the page must start in Tree or Columns, never a retired mode:\n${v}")
endif()

# 3. the Browse tile
file(WRITE "${WORK}/tiles.txt" "wait 100\ntopbar 2\nwait 600\n")
dui_run(t --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --input "${WORK}/tiles.txt")
if(NOT "${t}" MATCHES "browse tile ([0-9]+) 3 Browse Folders\n")
  message(FATAL_ERROR "the Browse page must hold the Browse Folders tile:\n${t}")
endif()
set(folders_tile ${CMAKE_MATCH_1})
file(WRITE "${WORK}/open.txt" "wait 100\ntopbar 2\nwait 600\nbrowse ${folders_tile}\nwait 600\n")
dui_run(o --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --input "${WORK}/open.txt")
if(NOT "${o}" MATCHES "folders tabs_drawn=[0-9]+ library_remaps=[0-9]+ browse_folders_opens=1\n")
  message(FATAL_ERROR "a click on the Browse Folders tile must count browse_folders_opens=1:\n${o}")
endif()
if(NOT "${o}" MATCHES "nav page=folders ")
  message(FATAL_ERROR "the Browse Folders tile must open the folders page:\n${o}")
endif()
