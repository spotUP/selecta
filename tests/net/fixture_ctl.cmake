# ctest fixture control for the net fixture server (D-M3-6).
# -DACTION=start|stop -DWORK=<dir> [-DCA_DIR=<dir with server.pem/server.key>]
# start writes WORK/port, WORK/tls_port, WORK/ftp_port, WORK/requests.jsonl.
find_program(PY python3 REQUIRED)
set(server "${CMAKE_CURRENT_LIST_DIR}/fixture_server.py")
if(ACTION STREQUAL "start")
  file(MAKE_DIRECTORY "${WORK}")
  set(tls)
  if(CA_DIR AND EXISTS "${CA_DIR}/server.pem")
    set(tls --tls-port-file "${WORK}/tls_port" --cert "${CA_DIR}/server.pem" --key "${CA_DIR}/server.key")
  endif()
  execute_process(COMMAND "${PY}" -I "${server}" start --portfile "${WORK}/port" --pidfile "${WORK}/pid"
    --logfile "${WORK}/requests.jsonl" --ftp-port-file "${WORK}/ftp_port" ${tls}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "fixture server start failed: ${out}${err}")
  endif()
  message(STATUS "${out}")
elseif(ACTION STREQUAL "stop")
  execute_process(COMMAND "${PY}" -I "${server}" stop --pidfile "${WORK}/pid")
else()
  message(FATAL_ERROR "fixture_ctl.cmake: ACTION must be start or stop")
endif()
