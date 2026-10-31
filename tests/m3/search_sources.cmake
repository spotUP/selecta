# I807: the Songs group of the search view lists songs of every source with hits, not only the first sources in key order.
#   -DSELECTA=<exe> -DCAT=<search_sources.tsv> -DWORK=<dir>
# The catalogue has 20 hvsc and 5 modland songs of one artist; hvsc sorts before modland, so the first 16 keys were all hvsc
# and a search for the artist showed no modland song. Both sources must show; the heading still counts all 25.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --catalogue "${CAT}"
          --exec "LIBRARY_SEARCH hubbard" --exec "VIEW_SOURCE search"
          --keys /dev/null --screen-dump "${WORK}/screen.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
file(READ "${WORK}/screen.txt" screen)
if(NOT screen MATCHES "Songs \\(25\\)")
  message(FATAL_ERROR "the Songs heading does not count the 25 hits\n${screen}")
endif()
if(NOT screen MATCHES "sidtune00")
  message(FATAL_ERROR "no hvsc song listed\n${screen}")
endif()
if(NOT screen MATCHES "modtune0")
  message(FATAL_ERROR "no modland song listed: the first sources in key order fill the group\n${screen}")
endif()
message(STATUS "search_sources ok")
