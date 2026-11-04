# dui-search-chip-scroll (I1073): the Search page source chips are one row that scrolls sideways with a chevron button at each
# end (the sidebar library chip band of I610/I669). At a 1280 px window the row overflows: Modules.pl lies past the right end,
# so its visible rect is empty and a click there picks nothing. One reachability test through the real shell (mem backend,
# scripted --input clicks on dump coordinates): -DSELECTA -DWORK -DNETWORK=<net fixture work dir>
#  1. at rest: `online_chips overflow=1 scrolls=0` and the modulespl chip is scrolled out (width 0);
#  2. the right chevron clicked until the row stops: `scrolls` (the chevron scroll count, the sentinel that the scroll ran)
#     is at least 1 and the modulespl chip is in view inside the band (between the chevrons);
#  3. a click on the centre of that chip picks it: `online scope=modulespl`.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1") # no real network is touched: the query stays empty, so a chip pick sends no search
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},radio.modules.pl=${fx},fixture.test=${fx}")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")

function(cs_run outvar input)
  file(REMOVE "${WORK}/library.db")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view search --canvas 1280x800 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# 1. at rest: the row overflows and Modules.pl is past the right end
cs_run(a "wait 300\n")
if(NOT "${a}" MATCHES "online_chips overflow=1 scrolls=0 lead=0 left -?[0-9]+ -?[0-9]+ [0-9]+ [0-9]+ right (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "at 1280 px the source chip row must overflow with chevrons and no scroll yet:\n${a}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ry "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
math(EXPR band_right "${CMAKE_MATCH_1}")
if(NOT "${a}" MATCHES "online_chip [0-9]+ -?[0-9]+ -?[0-9]+ 0 [0-9]+ scope=modulespl\n")
  message(FATAL_ERROR "at rest the Modules.pl chip should be scrolled out of the band (width 0):\n${a}")
endif()

# 2. the right chevron, clicked more times than there are chips: the row stops at its end
set(scroll "wait 300\n")
foreach(i RANGE 1 12)
  string(APPEND scroll "move ${rx} ${ry}\nclick ${rx} ${ry}\nwait 20\n")
endforeach()
cs_run(b "${scroll}")
if(NOT "${b}" MATCHES "online_chips overflow=1 scrolls=([0-9]+) ")
  message(FATAL_ERROR "no online_chips line after the chevron clicks:\n${b}")
endif()
if(CMAKE_MATCH_1 LESS 1 OR NOT CMAKE_MATCH_1 LESS 12)
  message(FATAL_ERROR "the chevron must scroll the row and stop at its end, scrolls=${CMAKE_MATCH_1}:\n${b}")
endif()
if(NOT "${b}" MATCHES "online_chip [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) scope=modulespl\n" OR CMAKE_MATCH_3 EQUAL 0)
  message(FATAL_ERROR "after scrolling the Modules.pl chip must be in view:\n${b}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
math(EXPR cend "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3}")
if(cend GREATER band_right)
  message(FATAL_ERROR "the Modules.pl chip (ends at ${cend}) runs under the right chevron (at ${band_right}):\n${b}")
endif()

# 3. a click on it picks Modules.pl
cs_run(c "${scroll}move ${cx} ${cy}\nclick ${cx} ${cy}\nwait 50\n")
if(NOT "${c}" MATCHES "online scope=modulespl ")
  message(FATAL_ERROR "a click on the scrolled-in Modules.pl chip must pick it:\n${c}")
endif()
if(NOT "${c}" MATCHES "online_chips overflow=1 scrolls=[1-9]")
  message(FATAL_ERROR "the pick must come after a chevron scroll (sentinel):\n${c}")
endif()
