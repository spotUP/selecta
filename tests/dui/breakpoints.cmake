# dui-breakpoints (DUI-P1-11, D-DUI-2): the layout edges are the decided 1100 and 900 logical pixels.
# -DSELECTA -DSONGS -DWORK
# Reachability: the product's own `layout width=... id=...` line (printed per --resize event by the real entry point) is the
# sentinel; every case below must print it with the expected id, so a layout that never ran fails the match.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
dui_run(out --ui desktop --backend mem --resize 1100x800,1099x800,900x800,899x800)
foreach(spec "1100:three-column" "1099:two-column" "900:two-column" "899:icon-sidebar")
  string(REPLACE ":" ";" pair "${spec}")
  list(GET pair 0 w)
  list(GET pair 1 id)
  if(NOT out MATCHES "layout width=${w} height=800 id=${id} ")
    message(FATAL_ERROR "width ${w} must give ${id}:\n${out}")
  endif()
endforeach()
