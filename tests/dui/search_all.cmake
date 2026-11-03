# dui-search-all (I1019e): the desktop Search page with NO source chip picked sends the fan-out line (SOURCE_SEARCH *) and draws one
# "From <name> (n)" section per searchable source after the library groups. One reachability test through the real shell, the
# scripted --input feeder (the typed query goes through the field and the debounce), against the recorded www.modules.pl fixture.
# -DSELECTA -DSONGS -DWORK -DNETWORK=<net fixture work dir>
#  1. no chip click: `search_section` lines for From Modules.pl with rows == the fixture's hits (3, the sentinel that the fan-out ran
#     and its answer reached the shared row loop), and the query reached the library too (`search groups` line present);
#  2. the From rows are song rows of the shared loop: `searchrow` lines carry their modulespl: keys.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},radio.modules.pl=${fx},fixture.test=${fx}")
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")

file(REMOVE_RECURSE "${WORK}/cfg")
file(REMOVE "${WORK}/library.db")
file(WRITE "${WORK}/in.txt" "wait 300\nkey c\nkey h\nkey i\nkey p\nkey enter\n")
foreach(i RANGE 1 40)
  file(APPEND "${WORK}/in.txt" "wait 300\n")
endforeach()
dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view search --canvas 1280x800 --clock fixed --stats
  --input "${WORK}/in.txt")
string(REGEX MATCHALL "search_section [0-9]+ rows=[0-9]+ [^\n]*" sections "${o}")
message(STATUS "sections: ${sections}")
if(NOT "${o}" MATCHES "search_section [0-9]+ rows=([0-9]+) From Modules.pl \\(([0-9]+)\\)\n")
  message(FATAL_ERROR "no 'From Modules.pl' section in the dump (no chip picked):\n${o}")
endif()
set(rows "${CMAKE_MATCH_1}")
set(count "${CMAKE_MATCH_2}")
if(NOT rows EQUAL 3 OR NOT count EQUAL 3)
  message(FATAL_ERROR "From Modules.pl: expected 3 rows and (3), got rows=${rows} (${count}):\n${o}")
endif()
if(NOT "${o}" MATCHES "search groups songs=")
  message(FATAL_ERROR "no library groups line in:\n${o}")
endif()
if(NOT "${o}" MATCHES "searchrow [0-9]+ [^\n]*key=modulespl:")
  message(FATAL_ERROR "the From rows are not song rows of the shared loop (no searchrow with a modulespl: key):\n${o}")
endif()
