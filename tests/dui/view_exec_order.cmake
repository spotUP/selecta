# dui-view-exec-order (I398): `--view <page>` enters its page BEFORE the `--exec` lines run, so a scripted setup survives.
# The real `selecta` entry point runs with --view library and the cover flow setup given only as --exec lines (VIEW_SOURCE queue,
# VIEW_WINDOW, VIEW_STYLE coverflow, COMPOSERS_SHOW); the input script is just Right and a wait. When the page entry came after the
# --exec lines it reset the view to the browse source with one row, VIEW_CURSOR +1 clamped at 0 and the Right key did nothing.
# Asserts: three Right keys move the cover flow centre from index 0 to index 3 (needs the 5 queue rows) and repaint.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-view-exec-order needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(setup "")
foreach(p 1.mod 2.mod 3.xm 4.it 5.sid)
  list(APPEND setup --exec "QUEUE_ADD \"fixture/${p}\"")
endforeach()
list(APPEND setup --exec "VIEW_STYLE regular" --exec "VIEW_SOURCE queue" --exec "VIEW_WINDOW 8" --exec "VIEW_STYLE coverflow" --exec "COMPOSERS_SHOW")

function(centre_of tag script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
    --exec "PREF_SET ui.desktop.reduce_motion 0" ${setup} --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "flow covers ([0-9]+)([^\n]*)\n")
    message(FATAL_ERROR "${tag}: no 'flow covers' line in:\n${o}")
  endif()
  set(${tag}_count "${CMAKE_MATCH_1}" PARENT_SCOPE)
  string(STRIP "${CMAKE_MATCH_2}" rest)
  string(REPLACE " " ";" rest "${rest}")
  list(GET rest -1 last) # draw order is far to near: the last entry is the centre cover
  string(REPLACE ":" ";" parts "${last}")
  list(GET parts 0 g)
  set(${tag}_centre "${g}" PARENT_SCOPE)
  if("${o}" MATCHES "ui_stats id=desktop frames=([0-9]+)")
    set(${tag}_frames "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endif()
endfunction()

centre_of(rest "wait 600\n")
centre_of(moved "key right\nwait 100\nkey right\nwait 100\nkey right\nwait 600\n")
message(STATUS "centre without a key ${rest_centre}, after Right ${moved_centre}; frames ${rest_frames} -> ${moved_frames}")
if(NOT rest_centre EQUAL 0)
  message(FATAL_ERROR "I398: the --exec setup did not survive --view library: centre cover is ${rest_centre}, want 0 of 5 queue rows")
endif()
if(NOT moved_centre EQUAL 3) # three rows deep: the browse source of the old order has fewer rows and clamps the cursor earlier
  message(FATAL_ERROR "I398: three Right keys did not move the cover flow to index 3: centre is ${moved_centre} (the --exec queue rows are missing)")
endif()
if(NOT moved_frames GREATER rest_frames)
  message(FATAL_ERROR "I398: Right did not repaint (frames ${moved_frames} vs ${rest_frames} without the key)")
endif()
