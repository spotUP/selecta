# dui-styles (DUI-P4-4): the engine view styles grid, coverflow and compact. -DSELECTA -DSONGS -DWORK
# 1. VIEW_STYLE grid, coverflow and compact are accepted on the desktop UI (it offers them through SEL_UI_VIEW_STYLES).
# 2. The rows carry artwork ids: albums.id of each fixture song's album (the songs fixture/1..5 (1.mod 2.mod 3.xm 4.it 5.sid) sit on albums 1,2,3,4,1).
# 3. The text UI offers the classic three styles only: VIEW_STYLE grid is refused with the existing unsupported_style error.
# The desktop draws grid and coverflow as artwork tiles (DUI-P4-5, test dui-grid-coverflow); compact stays a list.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-styles needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(queue "")
foreach(p 1.mod 2.mod 3.xm 4.it)
  list(APPEND queue --exec "QUEUE_ADD \"fixture/${p}\"")
endforeach()
list(APPEND queue --exec "QUEUE_ADD \"fixture/5.sid\"")
file(WRITE "${WORK}/in.txt" "wait 2\n")

function(style_run outvar ui style)
  file(REMOVE_RECURSE "${WORK}/cfg")
  # The desktop enters the named pages itself, in its first turn after every --exec (I266); the view source these
  # tests set by --exec must win, so the desktop gets no --view.
  set(view --view library)
  if(ui STREQUAL "desktop")
    set(view "")
  endif()
  dui_run(o --db "${db}" --ui ${ui} --backend mem ${view} --canvas 900x600 --clock fixed --stats ${queue}
    --exec "VIEW_SOURCE queue" --exec "VIEW_WINDOW 8" --exec "VIEW_STYLE ${style}" --input "${WORK}/in.txt")
  set(${outvar} "${o}\n${o_err}" PARENT_SCOPE)
endfunction()

foreach(style grid coverflow compact)
  style_run(dump desktop ${style})
  if(dump MATCHES "unsupported_style")
    message(FATAL_ERROR "desktop UI refused VIEW_STYLE ${style}:\n${dump}")
  endif()
  if(NOT dump MATCHES "vstyle ${style}\n")
    message(FATAL_ERROR "desktop dump lacks 'vstyle ${style}':\n${dump}")
  endif()
  set(i 0)
  foreach(art 1 2 3 4 1)
    if(NOT dump MATCHES "srow ${i} ${art}\\|")
      message(FATAL_ERROR "style ${style} row ${i} should carry artwork id ${art}:\n${dump}")
    endif()
    math(EXPR i "${i} + 1")
  endforeach()
endforeach()

style_run(text text grid)
if(NOT text MATCHES "error unsupported_style grid")
  message(FATAL_ERROR "text UI should refuse VIEW_STYLE grid with the existing unsupported_style error:\n${text}")
endif()
style_run(textok text tree)
if(textok MATCHES "unsupported_style")
  message(FATAL_ERROR "text UI should accept VIEW_STYLE tree:\n${textok}")
endif()

# Row artwork id follows a rescan: the rows are read (ids 1 2 3 4 1), then LIBRARY_SCAN stores a song, and a trigger on that
# insert moves every fixture song to an album that no albums row names (the scan changing the catalogue). The dumped ids
# must follow to 0; a row cache keyed by queue entry id alone keeps the stale 1 2 3 4 1.
set(scandir "${WORK}/scandir")
file(MAKE_DIRECTORY "${scandir}")
file(COPY "${HERE}/../m3/genre/ambient.mod" DESTINATION "${scandir}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}"
  "CREATE TRIGGER rescan_moves_albums AFTER INSERT ON songs WHEN NEW.path NOT LIKE 'fixture/%' BEGIN UPDATE songs SET album = 'Renamed' WHERE path LIKE 'fixture/%'; END;"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "creating the rescan trigger failed: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}/cfg")
dui_run(rs --db "${db}" --ui desktop --backend mem --canvas 900x600 --clock fixed --stats ${queue}
  --exec "VIEW_SOURCE queue" --exec "VIEW_WINDOW 8" --exec "VIEW_STYLE grid" --exec "LIBRARY_SCAN \"${scandir}\""
  --input "${WORK}/in.txt")
set(rs "${rs}\n${rs_err}")
foreach(i 0 1 2 3 4)
  if(NOT rs MATCHES "srow ${i} 0\\|")
    message(FATAL_ERROR "row ${i} artwork id should follow the rescan to 0:\n${rs}")
  endif()
endforeach()
