# I1218b: the desktop player-set gate. Inputs: -DSRC=<source dir> -DWORK=<scratch dir>.
# Runs cmake/player_set_check.cmake in child -P scripts: a desktop dev configure with SELECTA_GPL_PLUGINS=OFF or
# SELECTA_KSS_DRIVERS=OFF must fail naming the variable; every player on, the MIT release, a non-desktop host build
# and the Amiga build must pass.
set(MOD "${SRC}/cmake/player_set_check.cmake")
if(NOT EXISTS "${MOD}")
  message(FATAL_ERROR "player-set-check: ${MOD} is missing")
endif()
function(run_case name vars expect_rc expect_text)
  file(MAKE_DIRECTORY "${WORK}")
  file(WRITE "${WORK}/${name}.cmake" "${vars}\ninclude(\"${MOD}\")\nsel_player_set_check()\nmessage(STATUS \"player-set ok\")\n")
  execute_process(COMMAND "${CMAKE_COMMAND}" -P "${WORK}/${name}.cmake"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  set(all "${out}${err}")
  if(expect_rc EQUAL 0 AND NOT rc EQUAL 0 OR NOT expect_rc EQUAL 0 AND rc EQUAL 0)
    message(FATAL_ERROR "player-set-check ${name}: rc=${rc}, wanted ${expect_rc}\n${all}")
  endif()
  if(NOT "${expect_text}" STREQUAL "" AND NOT all MATCHES "${expect_text}")
    message(FATAL_ERROR "player-set-check ${name}: output lacks '${expect_text}'\n${all}")
  endif()
endfunction()
set(ALL_ON "set(SELECTA_GPL_PLUGINS ON)\nset(SELECTA_KSS_DRIVERS ON)")
run_case(desktop_gpl_off "set(SELECTA_UI_DESKTOP ON)\nset(SELECTA_GPL_PLUGINS OFF)\nset(SELECTA_KSS_DRIVERS OFF)" 1
  "SELECTA_GPL_PLUGINS=OFF")
run_case(desktop_kss_off "set(SELECTA_UI_DESKTOP ON)\nset(SELECTA_GPL_PLUGINS ON)\nset(SELECTA_KSS_DRIVERS OFF)" 1
  "SELECTA_KSS_DRIVERS=OFF")
run_case(desktop_all_on "set(SELECTA_UI_DESKTOP ON)\n${ALL_ON}" 0 "player-set ok")
run_case(desktop_mit_release "set(SELECTA_UI_DESKTOP ON)\nset(SELECTA_RELEASE_MIT_ONLY ON)" 0 "player-set ok")
run_case(host_no_desktop "set(SELECTA_GPL_PLUGINS OFF)" 0 "player-set ok")
run_case(amiga "set(SEL_AMIGA ON)\nset(SELECTA_UI_DESKTOP ON)" 0 "player-set ok")
message(STATUS "player-set-check: all cases passed")
