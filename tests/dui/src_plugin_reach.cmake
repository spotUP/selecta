# src-plugin-reach (I517 phase 3): a source that exists only as a plugin reaches the product.
#  1. src_plugin_reach_test drives the engine command channel (SOURCES, LIBRARY_SYNC testsrc x, SOURCES), checks the
#     plugin's sentinel and the 3 songs, and leaves the library database behind.
#  2. The desktop UI on that database lists a testsrc source row with 3 songs (the `source` lines of the stats dump).
#  0. (I1025) Without $SELECTA_PLUGIN_PATH the selecta binary lists no testsrc: it is a module, never linked in.
#  3. The id "testsrc" appears nowhere in src/, plugins/, include/ or apps/: core never names the source.
# -DSELECTA -DWORK -DGOLDEN -DSDL_BUILT -DREACH_TEST=<exe> -DSRC=<source root> -DPLUGINS=<dir holding sel_testsrc module>
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(db "${WORK}/library.db")
# I1025: the plain binary, no plugin path: no testsrc row, and a positive control that the source rows are printed at all.
unset(ENV{SELECTA_PLUGIN_PATH})
dui_run(plain --db "${WORK}/plain.db" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
if(NOT plain MATCHES "source [0-9]+ modland\\|")
  message(FATAL_ERROR "the source rows are missing from the plain run (control):\n${plain}")
endif()
if(plain MATCHES "testsrc")
  message(FATAL_ERROR "testsrc is registered in selecta without SELECTA_PLUGIN_PATH:\n${plain}")
endif()
if(NOT EXISTS "${PLUGINS}")
  message(FATAL_ERROR "no testsrc module directory: ${PLUGINS}")
endif()
set(ENV{SELECTA_PLUGIN_PATH} "${PLUGINS}")
set(ENV{SELECTA_TESTSRC_CALLS} "${WORK}/calls")
execute_process(COMMAND "${REACH_TEST}" "${db}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0 OR NOT out MATCHES "src_plugin_reach_ok")
  message(FATAL_ERROR "the engine half failed (rc ${rc}):\n${out}\n${err}")
endif()

file(GLOB_RECURSE scanned LIST_DIRECTORIES false "${SRC}/src/*" "${SRC}/plugins/*" "${SRC}/include/*" "${SRC}/apps/*")
foreach(f ${scanned})
  file(STRINGS "${f}" hit REGEX "testsrc" LIMIT_COUNT 1)
  if(hit)
    message(FATAL_ERROR "core names the test source: ${f}: ${hit}")
  endif()
endforeach()

if(NOT SDL_BUILT)
  message(STATUS "src-plugin-reach: no SDL build, the Sources page half is skipped")
  message("src_plugin_reach_ok")
  return()
endif()
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SOCKET} "/tmp/selecta-dui-src-plugin-reach.sock")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
file(REMOVE_RECURSE "${WORK}/cfg")
set(ENV{SELECTA_SDL_TEST_TIMED} "q@600")
dui_run(o0 --db "${db}" --ui desktop --backend sdl --view library --canvas 1280x800 --clock fixed --stats)
# I678i retired the Library Sources tab; the stats dump carries the per-source rows on every page (I786a).
if(NOT o0 MATCHES "source [0-9]+ testsrc\\|remote\\|Test Source\\|1\\|3\n")
  message(FATAL_ERROR "the desktop source rows show no testsrc row with 3 songs:\n${o0}")
endif()
message("src_plugin_reach_ok")
