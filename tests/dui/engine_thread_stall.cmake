# dui-engine-thread-stall (I1082a): -DSELECTA -DWORK -DSLOW=<tests/net/slow_server.py> -DMOD=<module> -DSDL_BUILT
# Red on main until I1082f (labels dui-red engine-thread-stall): a blocking engine open stops the desktop UI. Test body: engine_thread_stall.py.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
dui_require_sdl()
execute_process(COMMAND python3 -I "${HERE}/engine_thread_stall.py" "${SELECTA}" "${SLOW}" "${MOD}" "${WORK}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "${o}${e}")
endif()
message(STATUS "${o}")
