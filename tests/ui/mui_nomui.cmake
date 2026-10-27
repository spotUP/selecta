# mui-open-nomui (MUI-P1-4, Johan 2026-10-27): the host selecta is driven with --ui mui and no MUI probe (the host has none).
# The mui descriptor is registered, so main.c resolves it and calls mui_open once; mui_open prints the muimaster.library
# failure text; main.c then opens the text UI, which runs and quits on the key file. Inputs: SELECTA KEYS WORK.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${SELECTA}" --ui mui --output null --keys "${KEYS}" --screen-dump "${WORK}/screen.txt" --stats
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
string(REGEX MATCHALL "muimaster.library 19 \\(MUI 3.8\\) is not installed" hits "${err}")
list(LENGTH hits n)
if(NOT n EQUAL 1)
  message(FATAL_ERROR "RED[mui-open-nomui]: mui_open ran ${n} times (failure text count), want 1\nrc=${rc}\n${err}")
endif()
if(NOT out MATCHES "ui_waits=[0-9]+ flushes=[0-9]+ keys=[1-9]")
  message(FATAL_ERROR "RED[mui-open-nomui]: the text UI did not run after the MUI failure\nrc=${rc}\n${out}\n${err}")
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[mui-open-nomui]: exit ${rc}\n${out}\n${err}")
endif()
