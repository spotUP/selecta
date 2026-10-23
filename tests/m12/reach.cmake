# T28 selecta-m12-reach (plan 2026-10-23-m1-2-playlists-views-control.md, Phase 7).
# The M1.2 reachability test: the real shell, scripted keys, one run that loads a playlist,
# shows the queue as a tree with a user title format, swaps through a user binding, undoes,
# and opens and closes the palette. The shell prints
#   cmdline_runs=C palette_opens=P bindings=B playlist_entries=E undo_depth=U redo_depth=R view_style=S
# on exit; every number comes from the code that did the work.
#   -DSELECTA=<exe> -DMOD=<selecta-test-mk.mod> -DKEYS=<keys> -DBINDINGS=<bindings>
#   -DFIXTURE=<three.m3u> -DGOLDEN=<screen golden>
# A short fixed root keeps the folder names inside the screen width.
set(WORK "/tmp/selecta-m12-reach")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/tests/m12/a" "${WORK}/tests/m12/b" "${WORK}/config")
file(COPY "${FIXTURE}" DESTINATION "${WORK}/tests/m12")
file(COPY "${MOD}" DESTINATION "${WORK}/tests/m12/a")
file(COPY "${MOD}" DESTINATION "${WORK}/tests/m12/b")
file(RENAME "${WORK}/tests/m12/a/selecta-test-mk.mod" "${WORK}/tests/m12/a/one.mod")
file(COPY "${MOD}" DESTINATION "${WORK}/tests/m12/a")
file(RENAME "${WORK}/tests/m12/a/selecta-test-mk.mod" "${WORK}/tests/m12/a/two.mod")
file(RENAME "${WORK}/tests/m12/b/selecta-test-mk.mod" "${WORK}/tests/m12/b/three.mod")
file(WRITE "${WORK}/config/prefs.conf" "view.title_format=%title% - %format%\n")

if(NOT EXISTS "${SELECTA}")
  message(FATAL_ERROR "selecta shell not built: ${SELECTA}")
endif()

# Baseline: the default binding table, counted by a run that only quits.
file(WRITE "${WORK}/quit.keys" "q\n")
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --keys "${WORK}/quit.keys"
          --config-dir "${WORK}/config"
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE base_out ERROR_VARIABLE base_err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT base_out MATCHES "bindings=([0-9]+)")
  message(FATAL_ERROR "baseline run failed (${rc})\n${base_out}\n${base_err}")
endif()
set(default_bindings ${CMAKE_MATCH_1})

execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}" --keys "${KEYS}" --bindings "${BINDINGS}"
          --config-dir "${WORK}/config" --screen-dump "${WORK}/screen.txt"
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()

set(re "cmdline_runs=([0-9]+) palette_opens=([0-9]+) bindings=([0-9]+) playlist_entries=([0-9]+) undo_depth=([0-9]+) redo_depth=([0-9]+) view_style=([a-z]+)")
if(NOT out MATCHES "${re}")
  message(FATAL_ERROR "no cmdline_runs line\nstdout:\n${out}\nstderr:\n${err}")
endif()
math(EXPR want_bindings "${default_bindings} + 1")
set(got "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4} ${CMAKE_MATCH_5} ${CMAKE_MATCH_6} ${CMAKE_MATCH_7}")
set(want "3 1 ${want_bindings} 3 1 1 tree")
if(NOT got STREQUAL want)
  message(FATAL_ERROR "counters (cmdline_runs palette_opens bindings playlist_entries undo_depth redo_depth view_style)\n  got:  ${got}\n  want: ${want}")
endif()

file(READ "${WORK}/screen.txt" screen)
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "${got}")
