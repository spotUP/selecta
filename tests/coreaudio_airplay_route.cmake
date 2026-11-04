# I1039 reachability: headless selecta (desktop UI on the mem backend, the way the dui tests run it) with the CoreAudio
# output and the "AirPlay Speakers" route entry picked (output.device=selecta:airplay-route) plays through the
# AVSampleBufferAudioRenderer path. Sentinel: out_av_buffers in the --stats dump, the count of sample buffers the AV path
# queued; the HAL path never moves it. The mem backend's `wait` moves a virtual UI clock, so the whole run lasts about
# 0.3 s of wall time; measured 2026-11-03, the AV clock played 10584 frames (0.24 s) over that 0.34 s, i.e. real time.
# The 2-7 buffers seen before were this short run, not a starved pump. A wall-clock lower bound needs a real-time script
# verb (I1039 report, next steps). A SEEK must reach OUT_FLUSH (out_av_flushes).
# VOLUME 0 first, so nothing is heard. Inputs: -DSELECTA, -DMOD, -DWORK.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\n")
configure_file("${MOD}" "${WORK}/song.mod" COPYONLY)
file(WRITE "${WORK}/in.txt" "sleep 1000\nexec SEEK 1000\nsleep 1000\n")
string(TIMESTAMP t0 "%s")
execute_process(
  COMMAND "${SELECTA}" --output coreaudio --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend mem --view home
    --canvas 1280x800 --stats --exec "VOLUME 0" --exec "PREF_SET output.device selecta:airplay-route"
    --exec "QUEUE_ADD ${WORK}/song.mod" --exec "PLAY" --input "${WORK}/in.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
string(TIMESTAMP t1 "%s")
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
set(bufs "${CMAKE_MATCH_1}")
# Upper bound: the whole run's wall time at 48 kHz plus the 200 ms queued ahead, so an unpaced pump that floods the
# renderer fails.
math(EXPR hi "(${t1} - ${t0} + 1) * 48000 / 1024 + 10")
if(bufs LESS 1 OR bufs GREATER hi)
  message(FATAL_ERROR "the AirPlay route path is not paced in real time: out_av_buffers=${bufs}, want 1..${hi}\n${out}\n${err}")
endif()
if(NOT out MATCHES "out_av_flushes=([1-9][0-9]*)")
  message(FATAL_ERROR "SEEK did not flush the AirPlay route queue (out_av_flushes=0)\n${out}")
endif()
message(STATUS "coreaudio-airplay-route: out_av_buffers=${bufs} (max ${hi}) out_av_flushes=${CMAKE_MATCH_1}")
