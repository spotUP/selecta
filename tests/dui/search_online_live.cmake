# dui-search-online-live (I817): on the live SDL loop, the rows of an online search are drawn as soon as the engine
# answers, with no input after Enter. -DSELECTA -DWORK -DNETWORK=<net fixture work dir> -DSDL_BUILT.
# dui-search-online drives the same search through the scripted --input feeder, which never takes the live idle turn;
# this test runs the real live loop (SDL dummy driver counted as a live display, SELECTA_SDL_TEST_LIVE=1):
#  1. the Modules.pl scope comes from the pref search.last_source (no chip click), "chip" is typed and Enter pressed
#     through SDL events pushed at open (SELECTA_SDL_TEST_EVENTS), then nothing but a quit at 9 s;
#  2. the fixture answers 3 rows after its 4 s first-byte delay (I816), long after the last input event;
#  3. the Search page drew them: `online ... rows=3 drawn=3 state=2` (drawn counts the rows the last frame painted, the
#     sentinel that a frame ran after the answer).
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
set(ENV{SELECTA_SDL_TEST_TIMED} "q@9000")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\nsearch.last_source=modulespl\n")
dui_run(o --db "${WORK}/library.db" --ui desktop --backend sdl --view search --canvas 1280x800 --stats)
if(NOT "${o}" MATCHES "online scope=([^ ]*) searches=([0-9]+) rows=([0-9]+) drawn=([0-9]+) state=([0-9]+)")
  message(FATAL_ERROR "no online line in:\n${o}")
endif()
set(on "scope=${CMAKE_MATCH_1} searches=${CMAKE_MATCH_2} rows=${CMAKE_MATCH_3} drawn=${CMAKE_MATCH_4} state=${CMAKE_MATCH_5}")
if(NOT CMAKE_MATCH_1 STREQUAL "modulespl" OR CMAKE_MATCH_2 LESS 1 OR NOT CMAKE_MATCH_3 EQUAL 3 OR NOT CMAKE_MATCH_5 EQUAL 2)
  message(FATAL_ERROR "the live search must reach the engine and come back with 3 rows, got '${on}':\n${o}\nstderr:\n${o_err}")
endif()
if(NOT CMAKE_MATCH_4 EQUAL 3)
  message(FATAL_ERROR "I817: the 3 rows arrived but no frame drew them without input, got '${on}':\n${o}\nstderr:\n${o_err}")
endif()
