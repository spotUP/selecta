# dui-playing-mark (I690): a song list marks the row of the song the engine plays, and the mark moves with playback (design doc
# 2026-10-29 section on track lists). -DSELECTA -DSONGS -DWORK
# The playlist page (the collection page of a playlist) prints `plplaying <pos>`: the position of the row drawn as playing, -1 when none.
# 1. Play on the page queues a, b, c and plays a: plplaying 0.
# 2. The footer Next button moves playback to b while the page stays open: plplaying 1. The page never read a click, so the mark
#    comes from the engine's song, not from a selection.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n a b c)
  configure_file("${SONGS}/selecta-test.mod" "${WORK}/${n}.mod" COPYONLY)
endforeach()
set(db "${WORK}/lib.db")
function(centre outvar out re)
  if(NOT "${out}" MATCHES "${re}([0-9]+)[ ,]([0-9]+)[ ,]([0-9]+)[ ,]([0-9]+)")
    message(FATAL_ERROR "no '${re}' in:\n${out}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${outvar} "${cx} ${cy}" PARENT_SCOPE)
endfunction()
function(playing_of outvar out)
  if(NOT "${out}" MATCHES "plplaying (-?[0-9]+)\n")
    message(FATAL_ERROR "no plplaying line in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

dui_run(seed --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}"
  --exec "PLAYLIST_CREATE alpha" --exec "PLAYLIST_ADD_SONG 1 ${WORK}/a.mod" --exec "PLAYLIST_ADD_SONG 1 ${WORK}/b.mod" --exec "PLAYLIST_ADD_SONG 1 ${WORK}/c.mod")
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x800)
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(idle ${common} --input "${WORK}/idle.txt")
centre(side "${idle}" "sidelib_row 3 ")
file(WRITE "${WORK}/open.txt" "click ${side}\nwait 100\n")
dui_run(opened ${common} --input "${WORK}/open.txt")
playing_of(none "${opened}")
if(NOT none EQUAL -1)
  message(FATAL_ERROR "nothing plays, yet plplaying=${none}:\n${opened}")
endif()
centre(play "${opened}" "plpage id=[0-9]+ [^\n]* play=")

# Play, then Next: the layout output lists the footer buttons.
dui_run(lay --ui desktop --backend mem --canvas 1280x800 --resize 1280x800)
if(NOT "${lay}" MATCHES "button id=next x=(-?[0-9]+) y=(-?[0-9]+) w=([0-9]+) h=([0-9]+)")
  message(FATAL_ERROR "no next button in:\n${lay}")
endif()
math(EXPR nx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ny "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")

file(WRITE "${WORK}/p1.txt" "click ${side}\nwait 100\nclick ${play}\nwait 300\n")
dui_run(p1 ${common} --input "${WORK}/p1.txt")
playing_of(first "${p1}")
if(NOT first EQUAL 0)
  message(FATAL_ERROR "after Play the first row must be marked, plplaying=${first}:\n${p1}")
endif()
# The click on Next lands on the engine's next pump, so the wait after it is long.
file(WRITE "${WORK}/p2.txt" "click ${side}\nwait 100\nclick ${play}\nwait 300\nclick ${nx} ${ny}\nwait 600\n")
dui_run(p2 ${common} --input "${WORK}/p2.txt")
playing_of(second "${p2}")
if(NOT second EQUAL 1)
  message(FATAL_ERROR "after Next the second row must be marked, plplaying=${second}:\n${p2}")
endif()
