# dui-governor (DUI-P1-9, M4-P2-6): through the real shell, the effects governor decides what the desktop UI runs.
#   -DSELECTA=<exe> -DSONGS=<dir with the generated .mod files> -DWORK=<dir>
# Both runs play the same songs and change the song with NEXT, so the title cross-fade, the cover blur and the panel
# shadow all have a reason to run. rgba32 must run them (the sentinel proves the code ran); index8 must run none.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
set(base --ui desktop --backend mem --stats
  --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec "QUEUE_ADD ${SONGS}/selecta-short.mod"
  --exec "QUEUE_ADD ${SONGS}/selecta-loop.mod" --exec PLAY --exec NEXT)
dui_run(rgb ${base} --format rgba32)
dui_run(idx ${base} --format index8)
foreach(run rgb idx)
  dui_stat(${run}_frames "${${run}}" desktop frames)
  dui_stat(${run}_tweens "${${run}}" desktop tweens)
  dui_stat(${run}_prims "${${run}}" desktop effect_prims)
  if(${run}_frames LESS 2)
    message(FATAL_ERROR "${run}: the UI never redrew after NEXT (frames=${${run}_frames})")
  endif()
endforeach()
if(rgb_tweens EQUAL 0 OR rgb_prims EQUAL 0)
  message(FATAL_ERROR "rgba32 must run tweens and blur or shadow (tweens=${rgb_tweens} prims=${rgb_prims}); the sentinel never moved")
endif()
if(NOT idx_tweens EQUAL 0 OR NOT idx_prims EQUAL 0)
  message(FATAL_ERROR "index8 must run no tween, blur or shadow (tweens=${idx_tweens} prims=${idx_prims})")
endif()
# A format that is neither is refused.
execute_process(COMMAND "${SELECTA}" --output null --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend mem
  --format rgb565 --exec STATUS RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  message(FATAL_ERROR "an unknown --format must fail")
endif()
