# I214 driver: `selecta-cli config-dir` on the 68k binary under vamos.
# Case 1: ENVARC: unassigned -> returns within the timeout and names PROGDIR:Selecta (the fallback;
#         before the fix the code returned ENVARC:Selecta, which on a real Amiga opens an invisible
#         "insert volume" requester when ENVARC: is missing).
# Case 2: ENVARC: assigned -> ENVARC:Selecta.
# vamos does not emulate requesters, so the hang itself is only reproducible on a real machine (rig).
# Inputs: -DVAMOS -DAMIGA_CLI
foreach(v VAMOS AMIGA_CLI)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_config_dir.cmake: -D${v} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/vamos_cmd.cmake")
sel_vamos_cmd(SEL_VAMOS_CMD "${AMIGA_CLI}")
execute_process(COMMAND ${SEL_VAMOS_CMD} -- "${AMIGA_CLI}" config-dir
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "config-dir (ENVARC: unassigned): exit '${rc}'\n${out}${err}")
endif()
if(NOT out STREQUAL "config_dir=PROGDIR:Selecta\n")
  message(FATAL_ERROR "ENVARC: unassigned must fall back to PROGDIR:Selecta, got: ${out}")
endif()

execute_process(COMMAND ${SEL_VAMOS_CMD} -V cfgvol:${CMAKE_CURRENT_LIST_DIR} -a ENVARC:cfgvol:
  -- "${AMIGA_CLI}" config-dir
  OUTPUT_VARIABLE out2 ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0 OR NOT out2 STREQUAL "config_dir=ENVARC:Selecta\n")
  message(FATAL_ERROR "ENVARC: assigned must give ENVARC:Selecta, got '${out2}' rc '${rc}'\n${err}")
endif()
message(STATUS "[OK] config dir: PROGDIR:Selecta without ENVARC:, ENVARC:Selecta with it")
