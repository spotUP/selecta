# Shared helper of the real dui-* shell tests: run selecta and fail on a non-zero exit.
#   dui_run(<out var> <args...>)  needs SELECTA and WORK; adds --output null --size 80x25 --config-dir <WORK>/cfg.
#   dui_stat(<out var> <stdout> <ui id> <key>) reads key=value from the `ui_stats id=<ui id>` line.
function(dui_run outvar)
  file(MAKE_DIRECTORY "${WORK}")
  # I527: the sync-on-launch banner shifts the Library layout once the launch SOURCES reply arrives, and the fixtures hold
  # never-synced remote sources. Tests that are not about the banner start with it switched off (library.sync_stale_days=0);
  # a test that wants it sets DUI_SYNC_BANNER before calling dui_run.
  if(NOT DUI_SYNC_BANNER AND NOT EXISTS "${WORK}/cfg/prefs.conf")
    file(MAKE_DIRECTORY "${WORK}/cfg")
    file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
  endif()
  execute_process(
    COMMAND "${SELECTA}" --output null --size 80x25 --config-dir "${WORK}/cfg" ${ARGN}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "selecta exited ${rc}\nargs: ${ARGN}\nstdout:\n${out}\nstderr:\n${err}")
  endif()
  set(${outvar} "${out}" PARENT_SCOPE)
  set(${outvar}_err "${err}" PARENT_SCOPE)
endfunction()

function(dui_stat outvar out ui key)
  # The key must start a field: "dirty_pixels" must not match the tail of "page_dirty_pixels=" (greedy [^\n]* took the last).
  if(NOT "${out}" MATCHES "ui_stats id=${ui} ([^\n]* )?${key}=([0-9a-f]+)")
    message(FATAL_ERROR "no ${key} for ui '${ui}' in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

# I678i: the Library page (and its Composers rows) is retired; a composer's collection page opens from the Browse page's
# composer tile. dui_browse_composer_input(<out var> <prefix> <dump of a run of <prefix>topbar 2>) returns the input script that
# repeats <prefix> and opens the first composer tile (`browse <n>`, tile kind 1 on the `browse tile <n> <kind> <label>` lines).
function(dui_browse_composer_input outvar prefix dump)
  if(NOT "${dump}" MATCHES "browse tile ([0-9]+) 1 ")
    message(FATAL_ERROR "no composer tile on the Browse page:\n${dump}")
  endif()
  set(${outvar} "${prefix}topbar 2\nwait 3\nbrowse ${CMAKE_MATCH_1}\nwait 3\n" PARENT_SCOPE)
endfunction()
