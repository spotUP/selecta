# dui-footer (I312): the footer volume bar and seek bar take pointer input, through the real shell and --input feeder.
#   -DSELECTA -DSONGS -DWORK
# The bar rectangles come from the "footer vol_bar x y w h seek_bar x y w h" line the page dumps (the rectangles
# footer_bar_pointer tests). selecta-test-mk.mod plays through openmpt, which knows the song length, so a seek has a
# target.
# 1. A click at the middle of the volume bar sets the volume to 50 and counts one vol_click.
# 2. A drag that starts on the volume bar sets the volume where it is released (a quarter of the bar: 25).
# 3. A click at the middle of the seek bar sends a SEEK: seek_path leaves 0 and seek_clicks is 1.
# 4. A click outside the bars changes neither counter.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
configure_file("${SONGS}/selecta-test-mk.mod" "${WORK}/song.mod" COPYONLY)

function(footer_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "wait 2\n${input}wait 1\n")
  dui_run(o --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${WORK}/song.mod" --exec "PLAY" --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(footer_expect out vol vclicks sclicks seeked)
  if(NOT "${out}" MATCHES "volume=([0-9]+)")
    message(FATAL_ERROR "no volume in:\n${out}")
  endif()
  if(NOT CMAKE_MATCH_1 EQUAL vol)
    message(FATAL_ERROR "volume=${CMAKE_MATCH_1}, expected ${vol} in:\n${out}")
  endif()
  if(NOT "${out}" MATCHES "vol_clicks=([0-9]+) seek_clicks=([0-9]+)")
    message(FATAL_ERROR "no footer click counters in:\n${out}")
  endif()
  if(NOT CMAKE_MATCH_1 EQUAL vclicks OR NOT CMAKE_MATCH_2 EQUAL sclicks)
    message(FATAL_ERROR "vol_clicks=${CMAKE_MATCH_1} seek_clicks=${CMAKE_MATCH_2}, expected ${vclicks} ${sclicks}")
  endif()
  if(NOT "${out}" MATCHES "seek_path=([0-9]+)")
    message(FATAL_ERROR "no seek_path in:\n${out}")
  endif()
  if(seeked AND CMAKE_MATCH_1 EQUAL 0)
    message(FATAL_ERROR "seek bar click sent no SEEK (seek_path=0)")
  elseif(NOT seeked AND NOT CMAKE_MATCH_1 EQUAL 0)
    message(FATAL_ERROR "seek_path=${CMAKE_MATCH_1} without a seek bar click")
  endif()
endfunction()

footer_run(base "")
if(NOT "${base}" MATCHES "footer vol_bar ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) seek_bar ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no footer bar rectangles in the dump:\n${base}")
endif()
set(vx ${CMAKE_MATCH_1})
set(vw ${CMAKE_MATCH_3})
math(EXPR vy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(sx ${CMAKE_MATCH_5})
set(sw ${CMAKE_MATCH_7})
math(EXPR sy "${CMAKE_MATCH_6} + ${CMAKE_MATCH_8} / 2")
footer_expect("${base}" 100 0 0 FALSE)

# 1. Volume click at the middle.
math(EXPR vmid "${vx} + ${vw} / 2")
footer_run(vclick "click ${vmid} ${vy}\n")
footer_expect("${vclick}" 50 1 0 FALSE)

# 2. Volume drag from the left edge to a quarter of the bar; the band is taller than the 4 pixel bar.
math(EXPR vq "${vx} + ${vw} / 4")
math(EXPR vy_off "${vy} + 5")
footer_run(vdrag "drag ${vx} ${vy_off} ${vq} ${vy_off}\n")
footer_expect("${vdrag}" 25 1 0 FALSE)

# 3. Seek click at the middle.
math(EXPR smid "${sx} + ${sw} / 2")
footer_run(sclick "click ${smid} ${sy}\n")
footer_expect("${sclick}" 100 0 1 TRUE)

# 4. A click well above the volume bar is not on it.
math(EXPR vy_far "${vy} - 40")
footer_run(miss "click ${vmid} ${vy_far}\n")
footer_expect("${miss}" 100 0 0 FALSE)
