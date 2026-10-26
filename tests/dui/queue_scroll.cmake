# dui-queue-scroll (DUI-P4-8, I258): the Queue page scrolls with the mouse wheel. Sixty entries are queued, far more rows than
# the page fits. Before any wheel the dump (the rows the page draws) stops well short of entry 40; after a wheel past the end (200 notches of 16 logical
# pixels, clamped as the songs list is) the last row 59 is drawn, row 0 is scrolled out, and the wheel
# redrew the page (sentinel `queue draws`).
#   -DSELECTA -DSONGS -DWORK
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
configure_file("${SONGS}/selecta-test.mod" "${WORK}/song.mod" COPYONLY)
set(adds "")
foreach(n RANGE 1 60)
  list(APPEND adds --exec "QUEUE_ADD ${WORK}/song.mod")
endforeach()

function(queue_run tag input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view queue --canvas 1280x800 --clock fixed --stats ${adds} --input "${WORK}/${tag}.txt")
  set(${tag}_out "${o}" PARENT_SCOPE)
endfunction()

queue_run(a "wait 100\n")
if(NOT "${a_out}" MATCHES "count 60")
  message(FATAL_ERROR "60 entries were not queued:\n${a_out}")
endif()
if(NOT "${a_out}" MATCHES "qrow 0 ")
  message(FATAL_ERROR "row 0 is not drawn before the wheel")
endif()
if("${a_out}" MATCHES "qrow 40 ")
  message(FATAL_ERROR "row 40 is already drawn on the first page, the test list is not longer than the page")
endif()

queue_run(b "move 600 400\nwheel 0 -200\nwait 100\n")
if(NOT "${b_out}" MATCHES "qrow 59 ")
  message(FATAL_ERROR "after the wheel the last row (59), beyond the first page, is not drawn:\n${b_out}")
endif()
if("${b_out}" MATCHES "qrow 0 ")
  message(FATAL_ERROR "after the wheel row 0 is still drawn (the page did not scroll)")
endif()
if(NOT "${b_out}" MATCHES "queue draws ([0-9]+) rows ([0-9]+)" OR CMAKE_MATCH_1 LESS 2)
  message(FATAL_ERROR "the wheel did not redraw the Queue page:\n${b_out}")
endif()
