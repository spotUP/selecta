# I219c: KPlayer's hardware start is refused cleanly under vamos. Exemption: vamos's exec.library
# answers OpenResource with 0 for every name ("OpenResource(ciaa.resource) ignored"), so kpl14.s
# (k_timerval .. k_err, ~line 612-690) finds no CIA to take a timer from and KP_INIT returns -1. The
# CIA interrupt cadence and audible playback are only provable on the rig (T56). What this test
# asserts is the path that vamos CAN prove: the registry picks kplayer, the route hands the output
# over (hw_handovers==1), the start is refused (hw_starts==0) with the output given back
# (replayer=none), the process neither crashes nor hangs (exit 0 on `q`), and the trace names the
# missing resource as the cause.
# Inputs: VAMOS AMIGA_SELECTA WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
# Minimal ProTracker module: 1084-byte header ending in "M.K." plus one empty pattern.
execute_process(COMMAND python3 -c "
import sys
d = bytearray(1084 + 1024)
d[0:5] = b'test\\0'
d[950] = 1
d[1080:1084] = b'M.K.'
d[42:44] = (2).to_bytes(2, 'big')
d[45] = 64
open(sys.argv[1], 'wb').write(d)" "${WORK}/songs/a.mod" RESULT_VARIABLE prc)
if(NOT prc EQUAL 0)
  message(FATAL_ERROR "RED[I219c]: could not write the test module")
endif()
file(WRITE "${WORK}/k.keys" "Down\nEnter\nwait 1000\nq\n")
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -V work:${WORK} -l exec:info -- "${AMIGA_SELECTA}"
    --size 80x25 --start-dir work:songs --keys work:k.keys
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 90)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[I219c]: Selecta under vamos exited ${rc}\n${err}")
endif()
m2_check(I219c "${out}" "hw_handovers==1,hw_starts==0,replayer==none")
if(NOT err MATCHES "OpenResource\\(ciab\\.resource\\) ignored")
  message(FATAL_ERROR "RED[I219c]: vamos did not trace the ciab.resource refusal; the cause changed.\n${err}")
endif()
message(STATUS "[OK] I219c")
