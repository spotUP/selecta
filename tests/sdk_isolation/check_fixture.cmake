# I228: sel_check_includes.cmake must reject a plugin that includes "../other/x.h" and accept one that stays inside
# its own directory and include/selecta. Inputs: SRC (project source dir).
set(FIX "${SRC}/tests/sdk_isolation")
function(run_check dir expect_fail)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DPLUGIN=fixture "-DOWN_DIR=${FIX}/${dir}/plugin" "-DSOURCES=${FIX}/${dir}/plugin/plugin.c"
      -DTHIRD_PARTY= "-DSDK_DIR=${SRC}/include/selecta" "-DSHARED_DIR=${SRC}/plugins/shared"
      -P "${SRC}/cmake/sel_check_includes.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(expect_fail)
    if(rc EQUAL 0)
      message(FATAL_ERROR "the include check accepted ${dir}/plugin.c, which includes \"../other/x.h\"")
    endif()
    if(NOT err MATCHES "other/x.h")
      message(FATAL_ERROR "the include check failed without naming the offending include: ${err}")
    endif()
  elseif(NOT rc EQUAL 0)
    message(FATAL_ERROR "the include check rejected ${dir}/plugin.c: ${err}")
  endif()
endfunction()
run_check(bad TRUE)
run_check(good FALSE)
message(STATUS "sdk_isolation_fixture_ok")
