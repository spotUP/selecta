# mui-exec-queue (I726): the shell's `--exec` lines reach the engine, and Play on an empty queue is not "Playing".
# Real entry point: the selecta binary under the headless desktop UI (mem backend), the same shell path `--ui mui` takes.
# 1) --exec "QUEUE_ADD <song>": playlist_entries=1 (queue length 1) and queue_version=1.
# 2) --exec "PLAY" with an empty queue: the closing status line is not Playing, the clock stays --:--.
# Inputs: SELECTA SONG WORK.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/in.txt" "wait 1\n")
set(common --ui desktop --backend mem --canvas 640x512 --clock fixed --output null --size 80x25
  --config-dir "${WORK}/cfg" --input "${WORK}/in.txt" --stats)
execute_process(COMMAND "${SELECTA}" ${common} --exec "QUEUE_ADD ${SONG}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[mui-exec-queue]: exit ${rc}\n${out}\n${err}")
endif()
if(NOT out MATCHES "playlist_entries=1 ")
  message(FATAL_ERROR "RED[mui-exec-queue]: --exec QUEUE_ADD did not queue the song (want queue length 1)\n${out}")
endif()
execute_process(COMMAND "${SELECTA}" ${common} --exec "PLAY"
  OUTPUT_VARIABLE out2 ERROR_VARIABLE err2 RESULT_VARIABLE rc2 TIMEOUT 60)
if(out2 MATCHES "status=Playing")
  message(FATAL_ERROR "RED[mui-exec-queue]: Play on an empty queue left the status Playing\n${out2}")
endif()
if(NOT out2 MATCHES "status=Stopped")
  message(FATAL_ERROR "RED[mui-exec-queue]: Play on an empty queue should leave status=Stopped\n${out2}")
endif()
