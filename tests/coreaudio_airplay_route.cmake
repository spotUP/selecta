# I1039 reachability: headless selecta (desktop UI on the mem backend, the way the dui tests run it) with the CoreAudio
# output and the "AirPlay Speakers" route entry picked (output.device=selecta:airplay-route) plays through the
# AVSampleBufferAudioRenderer path. Sentinel: out_av_buffers in the --stats dump, the count of sample buffers the AV path
# queued; the HAL path never moves it. VOLUME 0 first, so nothing is heard. Inputs: -DSELECTA, -DMOD, -DWORK.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
configure_file("${MOD}" "${WORK}/song.mod" COPYONLY)
file(WRITE "${WORK}/in.txt" "wait 2\nwait 1\n")
execute_process(
  COMMAND "${SELECTA}" --output coreaudio --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend mem --view home
    --canvas 1280x800 --stats --exec "VOLUME 0" --exec "PREF_SET output.device selecta:airplay-route"
    --exec "QUEUE_ADD ${WORK}/song.mod" --exec "PLAY" --input "${WORK}/in.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(rc EQUAL 77)
  message(STATUS "coreaudio-airplay-route: no output device available")
  return()
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
if(NOT out MATCHES "out_av_buffers=([0-9]+)")
  message(FATAL_ERROR "no out_av_buffers in the --stats dump\nstdout:\n${out}")
endif()
if(CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "the AirPlay route path never queued a sample buffer: out_av_buffers=${CMAKE_MATCH_1}\n${out}")
endif()
message(STATUS "coreaudio-airplay-route: out_av_buffers=${CMAKE_MATCH_1}")
