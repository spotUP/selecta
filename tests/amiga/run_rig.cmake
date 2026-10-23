# Rig driver for the M2 rig tests T56, T59, T60 (plan 2026-10-24-m2-amiga.md, row I106).
# The rig is FS-UAE driven by ~/Code/UP-Term/vtcon/tools/rig (`rig.py start`); the `amiga` MCP server is
# wasabi's client of the same wasabid. A test script cannot call MCP tools, so this driver uses the MCP's
# sibling: `ami.py` (ping | exec | put | get | type | key), the rig's own CLI over the same wasabid socket.
#
# Rig down: prints `SKIPPED rig not running` and exits 0; the ctest entries carry
#   SKIP_REGULAR_EXPRESSION "SKIPPED rig not running" so ctest reports Skipped, never Passed.
# Rig up: runs the scenario for TAG; a missing binary or sentinel is a red failure.
#
# Inputs: TAG (T56|T58|T59|T60)  RIG_DIR  SELECTA (Amiga binary)  SELECTA_CLI (Amiga selecta-cli)  MOD (selecta-test.mod)  WORK (scratch dir)
find_program(PYTHON3 python3)
if(NOT PYTHON3 OR NOT EXISTS "${RIG_DIR}/ami.py")
  message(STATUS "SKIPPED rig not running (python3 or ${RIG_DIR}/ami.py not found)")
  return()
endif()

# Rig up = rig.py status does not say down and ami.py ping answers.
execute_process(COMMAND "${PYTHON3}" "${RIG_DIR}/rig.py" status
  OUTPUT_VARIABLE status ERROR_VARIABLE status_err RESULT_VARIABLE status_rc TIMEOUT 30)
execute_process(COMMAND "${PYTHON3}" "${RIG_DIR}/ami.py" ping
  OUTPUT_VARIABLE ping_out ERROR_VARIABLE ping_err RESULT_VARIABLE ping_rc TIMEOUT 30)
if(NOT status_rc EQUAL 0 OR status MATCHES "down" OR NOT ping_rc EQUAL 0)
  message(STATUS "SKIPPED rig not running (${TAG}: status='${status}' ping rc=${ping_rc}; start: python3 ${RIG_DIR}/rig.py start)")
  return()
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
include("${CMAKE_CURRENT_LIST_DIR}/../m2/lib.cmake")

# ami(out_var <args...>): one ami.py call, must succeed.
function(ami out_var)
  execute_process(COMMAND "${PYTHON3}" "${RIG_DIR}/ami.py" ${ARGN}
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 600)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "RED[${TAG}]: ami.py ${ARGN} exited ${rc}\n${out}${err}")
  endif()
  set(${out_var} "${out}${err}" PARENT_SCOPE)
endfunction()

# fetch(var path): get a file from the Amiga into a variable.
function(fetch var path)
  ami(ignored get "${path}" "${WORK}/fetched")
  file(READ "${WORK}/fetched" text)
  set(${var} "${text}" PARENT_SCOPE)
endfunction()

# clean(): drops what this script puts in RAM: (Selecta + selecta-cli are 25 MB; RAM: on the rig holds 24 MB free).
# Runs before the put, so a run that stopped on a RED still gets cleared by the next one, and again after a pass.
function(clean)
  execute_process(COMMAND "${PYTHON3}" "${RIG_DIR}/ami.py" exec
    "Delete >NIL: RAM:Selecta RAM:selecta-cli RAM:selecta-test.mod QUIET" 10
    OUTPUT_QUIET ERROR_QUIET TIMEOUT 60)
endfunction()

if(NOT EXISTS "${SELECTA}")
  message(FATAL_ERROR "RED[${TAG}]: ${SELECTA} not built: apps/selecta has no Amiga build yet.")
endif()
clean()
ami(ignored put "${SELECTA}" "RAM:Selecta")
ami(ignored put "${MOD}" "RAM:selecta-test.mod")

if(TAG STREQUAL "T56")
  # Library view, keys only: Enter plays the first entry; after 5 s the stats file is read.
  ami(ignored exec "Run >NIL: RAM:Selecta --output paula --stats T:selecta-stats --start-dir RAM:" 10)
  ami(ignored key 13)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 5)
  ami(ignored type "q")
  fetch(stats "T:selecta-stats")
  m2_check(T56 "${stats}" "paula_ints>=225,paula_ints<=275,underruns==0,out==paula")
elseif(TAG STREQUAL "T58")
  # Task priority read back on real exec (vamos stubs SetTaskPri), row I117.
  ami(ignored put "${SELECTA_CLI}" "RAM:selecta-cli")
  ami(out exec "RAM:selecta-cli --dry-run PRI=5" 10)
  m2_check(T58 "${out}" "task_pri_requested==5,task_pri==5")
elseif(TAG STREQUAL "T59")
  # RENDER-kind song on AHI; swaps = elapsed_ms * rate / buffer_frames / 1000 +-10%.
  ami(ignored exec "RAM:Selecta OUTPUT=AHI --play RAM:selecta-test.mod --seconds 10 --stats T:selecta-stats" 60)
  fetch(stats "T:selecta-stats")
  m2_check(T59 "${stats}" "ahi_bufs>0,underruns==0,ahi_swaps_expected>0,buffer_frames_default_68020==1")
  m2_get("${stats}" ahi_bufs got)
  m2_get("${stats}" ahi_swaps_expected want)
  math(EXPR lo "${want} * 90 / 100")
  math(EXPR hi "${want} * 110 / 100")
  if(got LESS lo OR got GREATER hi)
    message(FATAL_ERROR "RED[T59]: ahi_bufs=${got}, wanted ${lo}..${hi}")
  endif()
elseif(TAG STREQUAL "T60")
  # Per-player batch on the no-FPU machine (rig.py start --a1200 --nofpu), one verdict per player.
  if(NOT "$ENV{SEL_RIG_NOFPU}" STREQUAL "1")
    message(FATAL_ERROR "RED[T60]: needs the no-FPU rig: python3 ${RIG_DIR}/rig.py start --a1200 --nofpu, then SEL_RIG_NOFPU=1")
  endif()
  ami(ignored exec "RAM:Selecta --budget-batch T:selecta-budget" 1800)
  fetch(budget "T:selecta-budget")
  m2_check(T60 "${budget}" "players_measured>=1,players_over_budget_unflagged==0")
  string(REGEX MATCHALL "tick_us_max=[0-9]+" ticks "${budget}")
  foreach(t IN LISTS ticks)
    string(REGEX REPLACE "tick_us_max=" "" us "${t}")
    if(us GREATER_EQUAL 20000)
      message(FATAL_ERROR "RED[T60]: a player has ${t}, over one 50 Hz tick, without SEL_RP_NEEDS_FLOAT")
    endif()
  endforeach()
else()
  message(FATAL_ERROR "run_rig.cmake: unknown TAG ${TAG}")
endif()
clean()
message(STATUS "[OK] ${TAG}")
