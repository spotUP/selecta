# mui-song-arg (I674): `selecta <UI args> <song>` opens and plays the song named on the command line, for every UI.
# The shell owns the positional song (apps/selecta/main.c is_song_argument). The host has no MUI, so the headless desktop
# UI (mem backend) runs under the shell; the code under test is the shell's, shared with `--ui mui`.
# Sentinels: songs_opened=1 and song=<the path>. A value of an option (`--start-dir <file>`) is not a song.
# Inputs: SELECTA SONG WORK.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(WRITE "${WORK}/in.txt" "wait 1\n")
set(common --ui desktop --backend mem --canvas 640x512 --clock fixed --output null --size 80x25
  --config-dir "${WORK}/cfg" --input "${WORK}/in.txt" --stats)
execute_process(COMMAND "${SELECTA}" ${common} "${SONG}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[mui-song-arg]: exit ${rc}\n${out}\n${err}")
endif()
if(NOT out MATCHES "songs_opened=1 ")
  message(FATAL_ERROR "RED[mui-song-arg]: the command line song was not opened (want songs_opened=1)\n${out}\n${err}")
endif()
string(FIND "${out}" "song=${SONG}" at)
if(at EQUAL -1)
  message(FATAL_ERROR "RED[mui-song-arg]: the open song is not ${SONG}\n${out}")
endif()
execute_process(COMMAND "${SELECTA}" ${common} --start-dir "${SONG}"
  OUTPUT_VARIABLE out2 ERROR_VARIABLE err2 RESULT_VARIABLE rc2 TIMEOUT 60)
if(out2 MATCHES "songs_opened=[1-9]")
  message(FATAL_ERROR "RED[mui-song-arg]: an option value was taken for a song\n${out2}")
endif()
