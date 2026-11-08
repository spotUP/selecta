# coreaudio-av-nowait (I1143): a song change on the AirPlay route path never stalls the UI thread.
# Johan's Mac froze for a while on every song change with output.device=selecta:airplay-route. A live sample put the main
# thread in ca_pump -> AVSampleBufferRenderSynchronizer setRate:time: -> AudioQueuePause -> AwaitIOCycle: the re-anchor
# after OUT_FLUSH ran on the UI thread and waited on the AirPlay device. The fix runs every renderer call (create aside)
# on the output's own thread; the UI thread only waits on the frame counter, bounded to two pump turns.
# Driven through the product entry point: headless selecta, desktop UI on the mem backend, CoreAudio output with the
# AirPlay route picked, two songs queued, a click on the footer Next button. SELECTA_CA_AV_FAKE_BLOCK_MS=500 swaps the
# renderer for one whose start and flush sleep 500 ms, as AudioQueuePause does on the live route.
# Asserts:
# 1. ui_gap_ms_max < 200: no UI turn waited on the renderer (before the fix: >= 500, one blocked setRate);
# 2. sentinels: songs_opened >= 2 (Next changed the song), out_av_flushes >= 1 (the change reached the renderer flush),
#    out_av_buffers >= 1 (the route path, not HAL, played).
# VOLUME 0 first, and the fake renderer plays nothing. Inputs: -DSELECTA, -DMOD, -DWORK.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\noutput.device=selecta:airplay-route\n")
configure_file("${MOD}" "${WORK}/a.mod" COPYONLY)
configure_file("${MOD}" "${WORK}/b.mod" COPYONLY)
set(common --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend mem --view home --canvas 1280x800)
# The footer Next button, from the layout dump (null output: nothing plays here).
execute_process(COMMAND "${SELECTA}" --output null ${common} --resize 1280x800
  OUTPUT_VARIABLE lay ERROR_VARIABLE lay_err RESULT_VARIABLE rc)
if(NOT "${lay}" MATCHES "button id=next x=(-?[0-9]+) y=(-?[0-9]+) w=([0-9]+) h=([0-9]+)")
  message(FATAL_ERROR "no next button in the layout (rc ${rc}):\n${lay}\n${lay_err}")
endif()
math(EXPR nx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ny "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
# `sleep` is wall-clock time (`wait` only moves the virtual UI clock): the renderer worker needs real time to queue.
file(WRITE "${WORK}/in.txt" "sleep 900\nclick ${nx} ${ny}\nsleep 1500\n")
set(ENV{SELECTA_CA_AV_FAKE_BLOCK_MS} 500)
execute_process(
  COMMAND "${SELECTA}" --output coreaudio ${common} --stats --exec "VOLUME 0"
    --exec "QUEUE_ADD ${WORK}/a.mod" --exec "QUEUE_ADD ${WORK}/b.mod" --exec "PLAY" --input "${WORK}/in.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
unset(ENV{SELECTA_CA_AV_FAKE_BLOCK_MS})
if(rc EQUAL 77)
  message(STATUS "coreaudio-av-nowait: no output device available")
  return()
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()
foreach(key ui_gap_ms_max songs_opened out_av_flushes out_av_buffers)
  if(NOT "${out}" MATCHES "${key}=([0-9]+)")
    message(FATAL_ERROR "no ${key} in the --stats dump:\n${out}")
  endif()
  set(${key} "${CMAKE_MATCH_1}")
endforeach()
if(songs_opened LESS 2 OR out_av_flushes LESS 1 OR out_av_buffers LESS 1)
  message(FATAL_ERROR "Next did not change the song on the AirPlay route path: songs_opened=${songs_opened} "
    "out_av_flushes=${out_av_flushes} out_av_buffers=${out_av_buffers}\n${out}\n${err}")
endif()
if(ui_gap_ms_max GREATER 199)
  message(FATAL_ERROR "the UI thread waited on the AirPlay renderer: ui_gap_ms_max=${ui_gap_ms_max}, want < 200\n${out}")
endif()
message(STATUS "coreaudio-av-nowait: ui_gap_ms_max=${ui_gap_ms_max} songs_opened=${songs_opened} "
  "out_av_flushes=${out_av_flushes} out_av_buffers=${out_av_buffers}")
