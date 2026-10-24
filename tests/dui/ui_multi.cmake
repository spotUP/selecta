# dui-ui-multi (DUI-P1-2): --ui desktop --ui text --exec NEXT. -DSELECTA -DSONGS -DWORK
# Both UIs draw (frames > 0) and both show the new title: the desktop close line names it and the text screen holds it.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
dui_run(out --ui desktop --ui text --backend mem --stats --screen-dump "${WORK}/screen.txt"
  --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec "QUEUE_ADD ${SONGS}/selecta-short.mod"
  --exec PLAY --exec NEXT)
foreach(ui desktop text)
  dui_stat(frames "${out}" ${ui} frames)
  if(frames EQUAL 0)
    message(FATAL_ERROR "ui '${ui}' drew no frame:\n${out}")
  endif()
endforeach()
if(NOT out MATCHES "desktop_ui closed frames=[0-9]+ title=selecta short")
  message(FATAL_ERROR "the desktop UI does not show the new title:\n${out}")
endif()
file(READ "${WORK}/screen.txt" screen)
string(FIND "${screen}" "selecta short" at)
if(at LESS 0)
  message(FATAL_ERROR "the text UI does not show the new title:\n${screen}")
endif()
