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
if(NOT TAG STREQUAL "M4RTG")
  clean()
  ami(ignored put "${SELECTA}" "RAM:Selecta")
  ami(ignored put "${MOD}" "RAM:selecta-test.mod")
endif()

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
elseif(TAG STREQUAL "M4RTG")
  # I450 / M4-P5-1: RTG first light through the real entry point (Selecta --ui desktop --backend rtg), run on rig 2
  # (uaegfx; ctest sets UPTERM_RIG=2). A real library (the genre test modules scanned on the Amiga) is shown on the
  # Library page. Sentinels printed by the product: `rtg: screen WxH depth D` (the backend opened a Picasso96 screen),
  # `ui_backend ... backend=rtg` (the stats name the backend that ran), the composer row `|Unknown composer|4`
  # (the library rows reached the page). The product runs until Ctrl-C (Break), which closes the screen cleanly.
  set(B "SYS:selecta-test")
  file(WRITE "${WORK}/rtg.script" "Stack 200000\n${B}/Selecta --ui desktop --backend rtg --canvas 800x600 --view library --stats --db ${B}/library.db >RAM:rtg-out.txt\n")
  ami(ignored exec "MakeDir ${B} ${B}/mods" 10)
  ami(ignored exec "Delete RAM:rtg-out.txt ${B}/library.db QUIET" 10)
  ami(ignored put "${SELECTA}" "${B}/Selecta")
  ami(ignored put "${SELECTA_CLI}" "${B}/selecta-cli")
  file(GLOB genre_mods "${GENRE_DIR}/*.mod")
  foreach(m IN LISTS genre_mods)
    get_filename_component(mn "${m}" NAME)
    ami(ignored put "${m}" "${B}/mods/${mn}")
  endforeach()
  ami(scan exec "${B}/selecta-cli library scan --db ${B}/library.db ${B}/mods" 120)
  if(NOT scan MATCHES "db_songs=4")
    message(FATAL_ERROR "RED[M4RTG]: the Amiga scan did not find 4 songs: ${scan}")
  endif()
  ami(ignored put "${WORK}/rtg.script" "${B}/rtg.script")
  ami(ignored exec "Run >NIL: Execute ${B}/rtg.script" 10)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 15)
  ami(procs exec "Status" 10)
  string(REGEX MATCH "Process +([0-9]+): Loaded as command: ${B}/Selecta" hit "${procs}")
  if(hit STREQUAL "")
    message(FATAL_ERROR "RED[M4RTG]: Selecta is not running after 15 s (the UI did not open): ${procs}")
  endif()
  ami(ignored exec "Break ${CMAKE_MATCH_1} C" 10)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 5)
  fetch(out "RAM:rtg-out.txt")
  foreach(want "rtg: screen 800x600 depth" "ui_backend id=desktop backend=rtg" "|Unknown composer|4" "desktop_ui closed frames=")
    string(FIND "${out}" "${want}" pos)
    if(pos EQUAL -1)
      message(FATAL_ERROR "RED[M4RTG]: sentinel '${want}' missing in:\n${out}")
    endif()
  endforeach()
  ami(ignored exec "Delete RAM:rtg-out.txt QUIET" 10)
else()
  message(FATAL_ERROR "run_rig.cmake: unknown TAG ${TAG}")
endif()
clean()
message(STATUS "[OK] ${TAG}")
