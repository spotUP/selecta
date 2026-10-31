# shell-config-get (I729): a key in prefs.conf reaches the output plugin through the shell host. The wav output reads
# its file name with host->config_get("wav.path"); the stub host answered the default for every key, so the shell
# never opened it. Entry point: the selecta binary itself (--output wav --config-dir). Sentinel: the file the plugin
# opened exists and starts with RIFF. Inputs: SELECTA WORK.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "wav.path=${WORK}/out.wav\n")
file(WRITE "${WORK}/in.txt" "wait 1\n")
execute_process(COMMAND "${SELECTA}" --ui text --clock fixed --output wav --size 80x25
  --config-dir "${WORK}/cfg" --db "${WORK}/lib.db" --input "${WORK}/in.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
if(NOT EXISTS "${WORK}/out.wav")
  message(FATAL_ERROR "RED[shell-config-get]: wav.path from prefs.conf did not reach the wav output (rc ${rc})\n${out}\n${err}")
endif()
file(READ "${WORK}/out.wav" head LIMIT 4)
if(NOT head STREQUAL "RIFF")
  message(FATAL_ERROR "RED[shell-config-get]: out.wav is not a wav file")
endif()
