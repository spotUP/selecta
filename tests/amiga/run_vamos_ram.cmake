# I1305l driver: every vamos run from sel_vamos_cmd gets its own RAM: volume, so a second vamos on the
# machine (other ctest process, agent, hand run) or a dir left by a killed run cannot stop it.
# vamos' auto RAM: is the temp volume <HOME>/.vamos/volumes/ram: setup refuses an existing dir ("temp
# volume volume path already exists") and shutdown rmtree's it even after that refusal, which also
# deleted the RAM: (and T:) of the instance that was running. The driver runs under a scratch HOME so the
# user's real ~/.vamos is never touched, pre-creates that ram dir as a running instance would, and checks:
#   1. the run exits 0 with no temp volume error (before the fix: exit 1, "temp volume ... already exists")
#   2. the pre-existing dir and a marker file in it survive (before the fix: deleted by the failed run)
# Inputs: -DVAMOS -DAMIGA_CLI -DWORK (scratch dir, recreated)
foreach(v VAMOS AMIGA_CLI WORK)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_vamos_ram.cmake: -D${v} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK}")
set(busy "${WORK}/home/.vamos/volumes/ram")
file(MAKE_DIRECTORY "${busy}")
file(WRITE "${busy}/held-by-other-vamos" "x")

include("${CMAKE_CURRENT_LIST_DIR}/vamos_cmd.cmake")
sel_vamos_cmd(SEL_VAMOS_CMD "${AMIGA_CLI}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "HOME=${WORK}/home" ${SEL_VAMOS_CMD} -- "${AMIGA_CLI}" config-dir
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0 OR "${out}${err}" MATCHES "temp volume")
  message(FATAL_ERROR "vamos with a busy shared RAM: dir: exit '${rc}', must run on its own RAM:\n${out}${err}")
endif()
if(NOT EXISTS "${busy}/held-by-other-vamos")
  message(FATAL_ERROR "vamos deleted the RAM: dir of another instance (${busy})")
endif()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "[OK] vamos ram: private per run, shared RAM: dir untouched")
