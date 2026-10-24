# dui-draw (DUI-P2-1): the integer rasterizer, through the real shell and where it lives.
#   -DSELECTA=<exe> -DSONGS=<dir> -DWORK=<dir> -DGOLDEN=<tests/golden> -DUNIT=<draw_unit exe> -DGUISRC=<src/gui>
# 1. The primitive unit test passes on both canvas formats.
# 2. The test card, drawn by `--ui desktop --view testcard`, hashes to the golden (hash golden, plan 4.2; update only
#    with SELECTA_GOLDEN_UPDATE=1). draw_prims proves the rasterizer ran.
# 3. Host lint: no float or double anywhere under src/gui.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")

execute_process(COMMAND "${UNIT}" RESULT_VARIABLE urc OUTPUT_VARIABLE uout ERROR_VARIABLE uerr)
if(NOT urc EQUAL 0)
  message(FATAL_ERROR "draw_unit failed (${urc}):\n${uout}${uerr}")
endif()

foreach(fmt rgba32 index8)
  dui_run(out --ui desktop --backend mem --view testcard --canvas 640x400 --format ${fmt} --stats)
  dui_stat(prims "${out}" desktop draw_prims)
  dui_stat(hash "${out}" desktop canvas_hash)
  if(prims LESS 20)
    message(FATAL_ERROR "${fmt}: the test card ran only ${prims} primitives; the rasterizer sentinel never moved")
  endif()
  set(gfile "${GOLDEN}/desktop/testcard-${fmt}-640x400@1.hash")
  if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
    file(WRITE "${gfile}" "${hash}\n")
  endif()
  if(NOT EXISTS "${gfile}")
    message(FATAL_ERROR "missing golden ${gfile} (hash ${hash}); create it with SELECTA_GOLDEN_UPDATE=1")
  endif()
  file(STRINGS "${gfile}" want LIMIT_COUNT 1)
  if(NOT hash STREQUAL want)
    message(FATAL_ERROR "${fmt}: test card hash ${hash} differs from golden ${want} (${gfile})")
  endif()
endforeach()

file(GLOB_RECURSE gui_files "${GUISRC}/*.c" "${GUISRC}/*.h")
list(LENGTH gui_files gui_count)
if(gui_count LESS 2)
  message(FATAL_ERROR "lint found no source under ${GUISRC}")
endif()
foreach(f IN LISTS gui_files)
  file(STRINGS "${f}" lines)
  foreach(line IN LISTS lines)
    if(line MATCHES "(^|[^A-Za-z0-9_])(float|double)([^A-Za-z0-9_]|$)")
      message(FATAL_ERROR "floating point under src/gui: ${f}: ${line}")
    endif()
  endforeach()
endforeach()
