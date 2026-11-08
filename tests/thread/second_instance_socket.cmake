# second-instance-socket (I1162b): a second Selecta started while another one owns the control socket opens its own UI on the
# same library and runs without a control socket, instead of exiting with "already running".
# The text UI is used: the desktop UI runs the script clockless and the first instance would exit at once.
# Driven through the real selecta binary: the first instance holds SELECTA_SOCKET for 4 s (wait key); the second, same socket
# path and same --db, must
# 1. exit 0 and print the notice "another instance owns" (stderr), never "already running";
# 2. trace "second_instance_no_socket=1" (the sentinel: the new path ran);
# 3. leave the first instance's socket file in place (the second must not unlink or rebind it, also not on its exit).
# -DSELECTA -DWORK
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
file(WRITE "${WORK}/long.keys" "wait 4000\n")
file(WRITE "${WORK}/short.keys" "wait 200\n")
set(args --output null --size 80x25 --config-dir cfg --backend mem --db lib.db)
list(JOIN args " " a)
# The socket path is relative (an absolute build-dir path can exceed sun_path); both instances share it.
file(WRITE "${WORK}/run.sh" "
cd '${WORK}' || exit 1
SELECTA_SOCKET=ctl.sock
export SELECTA_SOCKET
'${SELECTA}' ${a} --keys long.keys >first.out 2>first.err &
pid=\$!
i=0
while [ ! -S ctl.sock ] && [ \$i -lt 100 ]; do sleep 0.1; i=\$((i + 1)); done
[ -S ctl.sock ] || { echo 'first instance never opened the socket'; kill \$pid; exit 3; }
'${SELECTA}' --stats ${a} --keys short.keys >second.out 2>second.err
rc=\$?
if [ -S ctl.sock ]; then echo socket_kept >>second.out; fi
wait \$pid
exit \$rc
")
execute_process(COMMAND sh "${WORK}/run.sh" RESULT_VARIABLE rc OUTPUT_VARIABLE o ERROR_VARIABLE e)
file(READ "${WORK}/second.err" se)
file(READ "${WORK}/second.out" so)
if(NOT rc EQUAL 0 OR se MATCHES "already running")
  message(FATAL_ERROR "second Selecta did not run beside the first (exit ${rc}):\n${o}${e}\n${se}")
endif()
if(NOT se MATCHES "another instance owns [^\n]*ctl.sock; this one runs without remote control")
  message(FATAL_ERROR "no notice line on stderr:\n${se}")
endif()
if(NOT se MATCHES "second_instance_no_socket=1")
  message(FATAL_ERROR "no second_instance_no_socket trace: the new path did not run:\n${se}")
endif()
if(NOT so MATCHES "socket_kept")
  message(FATAL_ERROR "the first instance's socket file is gone after the second exited")
endif()
message(STATUS "second-instance-socket: second instance ran without the socket, first socket kept")
