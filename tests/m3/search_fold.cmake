# I1406 text-ui-search-fold: / foldy Enter folds a 5-song category to 3 rows plus "Show all (5)";
# a second run presses Enter on the Songs heading (the cursor starts there) and expects all 5 rows and "Show less".
# Only the right pane is checked: the sidebar column repeats the unfolded list.
#   -DSELECTA=<exe> -DCAT=<catalogue_fold.tsv> -DKEYS=<search_fold.keys> -DWORK=<dir> -DCWD=<dir>
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${KEYS}" base)
function(run_keys name text)
  file(WRITE "${WORK}/${name}.keys" "${text}")
  execute_process(
    COMMAND "${SELECTA}" --output null --size 80x25 --start-dir "${CWD}" --catalogue "${CAT}"
            --keys "${WORK}/${name}.keys" --screen-dump "${WORK}/${name}.txt"
    WORKING_DIRECTORY "${CWD}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "${name}: selecta exited ${rc}\n${out}\n${err}")
  endif()
  file(READ "${WORK}/${name}.txt" s)
  set(${name}_screen "${s}" PARENT_SCOPE)
endfunction()
run_keys(folded "${base}")
if(NOT folded_screen MATCHES "│ +Show all \\(5\\)" OR folded_screen MATCHES "│   Fold Tune 4")
  message(FATAL_ERROR "folded screen wrong:\n${folded_screen}")
endif()
run_keys(open "${base}Enter\nwait 300\n")
if(NOT open_screen MATCHES "│ +Show less" OR NOT open_screen MATCHES "│   Fold Tune 5")
  message(FATAL_ERROR "expanded screen wrong:\n${open_screen}")
endif()
message(STATUS "search_fold ok")
