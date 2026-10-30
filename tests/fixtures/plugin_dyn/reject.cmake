# I427a: three bad modules (no entry symbol, entry answers NULL for the host API, duplicate plugin id of a
# built-in) are each skipped, counted in dyn_rejected and logged with path and reason; none is listed.
# Variables: CLI REJECT
set(ENV{SELECTA_PLUGIN_PATH} "${REJECT}")
execute_process(COMMAND "${CLI}" plugins --stats OUTPUT_VARIABLE list ERROR_VARIABLE log RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "plugins exited ${rc}\n${list}\n${log}")
endif()
if(NOT list MATCHES "dyn_loaded=0\ndyn_rejected=3\n")
  message(FATAL_ERROR "expected dyn_loaded=0 dyn_rejected=3:\n${list}")
endif()
if(list MATCHES "origin=${REJECT}")
  message(FATAL_ERROR "a rejected module is listed:\n${list}")
endif()
set(all "${list}\n${log}")
foreach(case "sel_dyn_nosym[^\n]*no sel_plugin_entry symbol"
             "sel_dyn_apimismatch[^\n]*entry returned NULL"
             "sel_dyn_dup[^\n]*duplicate plugin id ym")
  if(NOT all MATCHES "${case}")
    message(FATAL_ERROR "reject log lacks '${case}':\n${all}")
  endif()
endforeach()
