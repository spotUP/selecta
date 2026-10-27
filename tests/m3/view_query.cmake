# T66 view-library-source (plan 2026-10-25-m3-library.md, Phase 2, I151).
#   -DSELECTA=<exe> -DCAT=<catalogue.tsv> -DKEYS=<keys file> -DGOLDEN=<screen golden> -DWORK=<dir>
# Runs the shell twice over the same library query results shown as the view
# source: once with only the cursor move, once with the style cycled (KEYS).
# Both must end on the same cursor_key=, say view_source=query, and the cycled
# run's last screen must equal the golden.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(runs plain cycled)
foreach(run IN LISTS runs)
  set(keys "${KEYS}")
  if(run STREQUAL "plain")
    file(WRITE "${WORK}/plain.keys" "Down\nwait 500\n")
    set(keys "${WORK}/plain.keys")
  endif()
  execute_process(
    COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --catalogue "${CAT}"
            --exec "LIBRARY_SEARCH lizardking" --exec "VIEW_SOURCE query"
            --keys "${keys}" --screen-dump "${WORK}/${run}.txt"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${run}: selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  endif()
  if(NOT out MATCHES "view_source=query view_style=([a-z]+) cursor_key=([^\n]+)\n")
    message(FATAL_ERROR "${run}: no view_source=query line\n${out}")
  endif()
  set(style_${run} "${CMAKE_MATCH_1}")
  set(key_${run} "${CMAKE_MATCH_2}")
endforeach()
if(NOT style_cycled STREQUAL "regular" OR NOT style_plain STREQUAL "regular")
  message(FATAL_ERROR "style after the cycle: ${style_cycled} (plain ${style_plain}), want regular")
endif()
if(NOT key_plain STREQUAL "selecta-test.mod")
  message(FATAL_ERROR "cursor_key=${key_plain}, want the second result selecta-test.mod")
endif()
if(NOT key_cycled STREQUAL key_plain)
  message(FATAL_ERROR "the style switch lost the selected song: ${key_cycled} vs ${key_plain}")
endif()
file(READ "${WORK}/cycled.txt" screen)
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "view_source=query cursor_key=${key_cycled}")

# I151a: VIEW_SOURCE playlist (the queue) and home (I191: its own source with the three sections; the catalogue has no history) answer OK.
foreach(src IN ITEMS playlist home)
  execute_process(
    COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --catalogue "${CAT}"
            --exec "LIBRARY_SEARCH lizardking" --exec "VIEW_SOURCE ${src}"
            --keys "${WORK}/plain.keys" --screen-dump "${WORK}/${src}.txt"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0 OR err MATCHES "unsupported")
    message(FATAL_ERROR "VIEW_SOURCE ${src} failed (rc ${rc})\nstdout:\n${out}\nstderr:\n${err}")
  endif()
endforeach()
if(NOT out MATCHES "view_source=home")
  message(FATAL_ERROR "VIEW_SOURCE home did not land on the home source\n${out}")
endif()
file(READ "${WORK}/home.txt" home_screen)
if(NOT home_screen MATCHES "Home" OR NOT home_screen MATCHES "Recently played")
  message(FATAL_ERROR "home view should show its sections\n${home_screen}")
endif()
file(READ "${WORK}/playlist.txt" pl_screen)
if(NOT pl_screen MATCHES "Queue")
  message(FATAL_ERROR "VIEW_SOURCE playlist should show the queue\n${pl_screen}")
endif()
message(STATUS "view_source playlist and home answer OK")

# I569: VIEW_SOURCE search with no search run (res_count == 0) still lands on the search source and prints it.
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --catalogue "${CAT}"
          --exec "VIEW_SOURCE search" --keys "${WORK}/plain.keys" --screen-dump "${WORK}/search.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR err MATCHES "unsupported" OR NOT out MATCHES "view_source=search")
  message(FATAL_ERROR "VIEW_SOURCE search without a prior search did not land on the search source (rc ${rc})\nstdout:\n${out}\nstderr:\n${err}")
endif()
message(STATUS "view_source search with an empty result answers OK")
