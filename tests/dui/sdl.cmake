# dui-sdl (DUI-P2-7): the sdl backend presents the same frames as the mem backend. -DSELECTA -DSONGS -DWORK -DSDL_BUILT
#   SDL_VIDEODRIVER=dummy       no window opens (offscreen needs EGL, absent here); the test never needs a display
#   resize.txt                  `resize W H` and `tile N` directives, replayed by both backends
# 1. Without SELECTA_UI_DESKTOP the sdl backend is refused with the "not available in this build" message.
# 2. With it, mem and sdl run the same script: the sdl presented frame hash (read back from the renderer) equals the
#    mem canvas hash, and equals the sdl canvas hash.
# 3. Sentinels: presents > 0 (the sdl path ran), upload_pixels == dirty_total (only dirty rectangles went to the texture).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

if(NOT SDL_BUILT)
  execute_process(COMMAND "${SELECTA}" --output null --size 80x25 --config-dir "${WORK}/cfg" --ui desktop --backend sdl
    RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_VARIABLE out)
  if(rc EQUAL 0 OR NOT err MATCHES "not available in this build")
    message(FATAL_ERROR "sdl without SELECTA_UI_DESKTOP must fail with 'not available in this build' (rc=${rc}): ${err}")
  endif()
  return()
endif()

set(ENV{SDL_VIDEODRIVER} dummy)
file(WRITE "${WORK}/resize.txt" "resize 960 600\ntile 0\nresize 1280 800\nresize 800 640\ntile 1\n")
set(common --ui desktop --view home --canvas 1280x800 --clock fixed --stats --input "${WORK}/resize.txt"
  --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec "QUEUE_ADD ${SONGS}/selecta-test.mod")

dui_run(mem ${common} --backend mem)
dui_run(sdl ${common} --backend sdl)
dui_stat(mem_hash "${mem}" desktop canvas_hash)
dui_stat(sdl_hash "${sdl}" desktop canvas_hash)
dui_stat(shown "${sdl}" desktop present_hash)
dui_stat(presents "${sdl}" desktop presents)
dui_stat(uploaded "${sdl}" desktop upload_pixels)
dui_stat(painted "${sdl}" desktop dirty_total)
if(presents LESS 3)
  message(FATAL_ERROR "sdl presented ${presents} frames; the script needs at least 3 (sentinel)")
endif()
if(NOT mem_hash STREQUAL sdl_hash)
  message(FATAL_ERROR "canvas hash differs: mem ${mem_hash}, sdl ${sdl_hash}")
endif()
if(NOT shown STREQUAL mem_hash)
  message(FATAL_ERROR "presented frame hash ${shown} differs from the mem hash ${mem_hash}")
endif()
if(NOT uploaded EQUAL painted)
  message(FATAL_ERROR "sdl uploaded ${uploaded} pixels but the canvas painted ${painted}")
endif()
