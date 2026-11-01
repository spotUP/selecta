# dui-sync-offer-idle (I789): the sync offer at launch is painted in a live window that gets no input.
# -DSELECTA -DWORK -DSDL_BUILT. The offer opens in desk_wait_turn once the SOURCES rows arrive, outside any event. A live
# turn presents only when the ui model is dirty, so an open that set only `repaint` left the dialog unpainted until the
# next mouse move: on the Mac the window showed Home with no offer. Scripted runs hid it, because every scripted event
# flushes a frame. This runs the real live loop (SELECTA_SDL_TEST_LIVE=1) with no input at all before the timed quit and
# checks the stats: the offer opened ("nagdlg opens 1") and was painted ("nagdlg draws" at least 1).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
set(DUI_SYNC_BANNER ON) # I527: this test is about the sync offer (see dui_run.cmake)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
dui_require_sdl()
set(ENV{SDL_VIDEODRIVER} dummy)
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-sync-offer-idle needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}/cfg")
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SDL_TEST_TIMED} "q@4000")
set(ENV{SELECTA_SOCKET} "${WORK}/ctl.sock")
dui_run(o --db "${db}" --ui desktop --backend sdl --view home --canvas 1280x800 --clock fixed --stats)
if(NOT o MATCHES "nagdlg opens 1 done 1\n")
  message(FATAL_ERROR "no sync offer at launch:\n${o}")
endif()
if(NOT o MATCHES "nagdlg draws ([0-9]+)\n" OR CMAKE_MATCH_1 EQUAL 0)
  message(FATAL_ERROR "the sync offer opened but was never painted in an idle live window:\n${o}")
endif()
message(STATUS "sync offer idle: opens 1, draws ${CMAKE_MATCH_1}")
