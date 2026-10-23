# T58 amiga-tooltypes-cli: `selecta-cli --dry-run` with CLI args and with a ToolTypes fixture set the same prefs.
# Sentinels: opts= (parsed options), prefs_hash= (the resulting prefs), task_pri= (FindTask()->tc_Node.ln_Pri read back).
# The fixture is the ToolTypes text (one KEY=VALUE per line); the .info wrapper is the parser's job (I116).
# Inputs: MODE=vamos VAMOS AMIGA_CLI CLI WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/Selecta.tooltypes" "OUTPUT=AHI\nPRI=5\nBUFFER=4096\nLED=ON\n")
set(VAMOS_VOLUMES -V work:${WORK})
m2_run_ok(T58 cli --dry-run OUTPUT=AHI PRI=5 BUFFER=4096 LED=ON)
m2_check(T58 "${cli}" "opts==4,task_pri==5,output==ahi,buffer==4096,led==on")
m2_run_ok(T58 tt --dry-run --tooltypes work:Selecta.tooltypes)
m2_check(T58 "${tt}" "opts==4,task_pri==5")
m2_get("${cli}" prefs_hash a)
m2_get("${tt}" prefs_hash b)
if(NOT a STREQUAL b)
  message(FATAL_ERROR "RED[T58]: CLI prefs_hash=${a} differs from ToolTypes prefs_hash=${b}")
endif()
message(STATUS "[OK] T58 ${MODE}")
