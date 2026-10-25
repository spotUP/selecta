# I287: the 68k selecta must define its own __chkabort. libnix's default exit()s on Ctrl-C from inside stdio and skips
# the desktop UI close, so the RTG screen stayed open. A second definition would fail the link, so one in selecta's own
# main object means libnix's member is not linked.
# Inputs: -DNM=<m68k nm> -DMAIN_OBJ=<build/amiga selecta main.c object>.
execute_process(COMMAND "${NM}" "${MAIN_OBJ}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "no-libnix-abort: nm failed (rc=${rc})\n${err}")
endif()
if(NOT out MATCHES "[0-9a-f]+ T ___chkabort")
  message(FATAL_ERROR "no-libnix-abort: ${MAIN_OBJ} does not define __chkabort")
endif()
message(STATUS "no-libnix-abort: selecta defines its own __chkabort")
