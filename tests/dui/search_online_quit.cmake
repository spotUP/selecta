# dui-search-online-quit (I832): quitting while an online search waits on the network returns at once. The fixture delays the first
# byte of the chip search by 4 s (I816). The scripted --input feeder holds its turn until the answer, so this runs the live SDL loop
# like dui-search-online-live (I817): Enter, then a quit at 1.5 s, inside the delay, so the engine is destroyed mid-wait.
# sel_engine_destroy -> sel_library_task_cancel waits for the search task, which must see the cancel in its net wait
# (task_cancel as the net cancel generation), not sit out the whole answer. PASS: selecta exits in under 2.5 s, the search
# started (searches>=1, the sentinel that the worker ran and was cancelled mid-wait) and its answer never arrived (rows=0).
# -DSELECTA -DWORK -DNETWORK=<net fixture work dir> -DSDL_BUILT
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
if(NOT SDL_BUILT)
  return()
endif()
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},radio.modules.pl=${fx},fixture.test=${fx}")
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")

set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SDL_TEST_EVENTS} "tchip k0d")
set(ENV{SELECTA_SDL_TEST_TIMED} "q@1500")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\nsearch.last_source=modulespl\n")
string(TIMESTAMP t0 "%s%f")
dui_run(out --db "${WORK}/library.db" --ui desktop --backend sdl --view search --canvas 1280x800 --stats)
string(TIMESTAMP t1 "%s%f")
# %s%f is seconds then 6 digits of microseconds: one integer in microseconds.
math(EXPR us "${t1} - ${t0}")
math(EXPR ms "${us} / 1000")
if(NOT "${out}" MATCHES "online scope=([^ ]*) searches=([0-9]+) rows=([0-9]+) ")
  message(FATAL_ERROR "no online line in:\n${out}")
endif()
if(CMAKE_MATCH_2 LESS 1)
  message(FATAL_ERROR "the search never started (searches=${CMAKE_MATCH_2}); nothing was cancelled:\n${out}")
endif()
if(NOT CMAKE_MATCH_3 EQUAL 0)
  message(FATAL_ERROR "the answer arrived (rows=${CMAKE_MATCH_3}) before quit; the test no longer quits mid-wait:\n${out}")
endif()
message(STATUS "I832: run took ${ms} ms")
if(ms GREATER 3000)
  message(FATAL_ERROR "I832: quit during an online search took ${ms} ms; it waited for the network answer (fixture delay 4000 ms, quit at 1500 ms)")
endif()
