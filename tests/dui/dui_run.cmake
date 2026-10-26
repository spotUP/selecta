# Shared helper of the real dui-* shell tests: run selecta and fail on a non-zero exit.
#   dui_run(<out var> <args...>)  needs SELECTA and WORK; adds --output null --size 80x25 --config-dir <WORK>/cfg.
#   dui_stat(<out var> <stdout> <ui id> <key>) reads key=value from the `ui_stats id=<ui id>` line.
function(dui_run outvar)
  file(MAKE_DIRECTORY "${WORK}")
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
