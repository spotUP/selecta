# dui-hover-motion (I1067): a pointer move onto a song row hovers that row at once, with no scroll-wheel turn. -DSELECTA -DSONGS -DWORK
# Johan 2026-11-05 (Mac): song rows in every list showed no hover while the mouse moved; a wheel turn made it appear. The wheel
# forces a full repaint and list_row_fill reads the pointer position, but a move repaints only when desk_hover_move finds a new box
# in the pointer hit-test list (hbox[]). Root cause: hbox[] (cap 128) was never emptied; only the main region pruned, and only
# boxes inside its new rect. Each window resize or zoom left the old layout's boxes behind until the list was full, and hover_reg
# dropped the rows of the new layout: a move found no row (or a stale one) and repainted nothing. Fix: desk_present empties the
# list on every whole-canvas repaint, since every region registers its controls again then.
# Entry point: scripted pointer input through the real desktop UI (--input). Sentinel: the `hover_box x y w h` dump line, which only
# desk_hover_move sets, from the hit-test list. The `plhover fill` line is no proof: the mem backend repaints the whole frame, so the
# fill follows the pointer even when the move found nothing.
# 1. Playlist page, pointer moves onto song row 1: hover_box is the drawn row inside the `plsong 1` slot.
# 2. Moves over the sidebar and out of the window, then onto row 2: hover_box is the drawn row of `plsong 2`, and the
#    `hover_boxes n=` count stays under its cap.
# 3. Twenty resizes and back, then a move onto row 1: hover_box is the drawn row 1 and the list is under its cap (fails without
#    the fix: a stale box of an old layout, n=128 of 128).
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
function(row_of outvar out n)
  if(NOT "${out}" MATCHES "plsong ${n} (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) ")
    message(FATAL_ERROR "no 'plsong ${n}' in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4}" PARENT_SCOPE)
endfunction()
function(hover_of outvar out)
  if(NOT "${out}" MATCHES "hover_box (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) ")
    message(FATAL_ERROR "no hover_box line in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4}" PARENT_SCOPE)
endfunction()
# The hover box is the drawn row (the row slot drawn with the list padding, a few pixels off the slot): it must cover the
# middle of the slot, where the pointer is, and be at least half the slot wide.
function(hover_in outvar box row)
  string(REPLACE " " ";" b "${box}")
  string(REPLACE " " ";" r "${row}")
  list(GET b 0 bx)
  list(GET b 1 by)
  list(GET b 2 bw)
  list(GET b 3 bh)
  list(GET r 0 rx)
  list(GET r 1 ry)
  list(GET r 2 rw)
  list(GET r 3 rh)
  math(EXPR cx "${rx} + ${rw} / 2")
  math(EXPR cy "${ry} + ${rh} / 2")
  math(EXPR bx2 "${bx} + ${bw}")
  math(EXPR by2 "${by} + ${bh}")
  math(EXPR half "${rw} / 2")
  set(ok 0)
  if(NOT bx GREATER cx AND bx2 GREATER cx AND NOT by GREATER cy AND by2 GREATER cy AND bw GREATER half AND NOT bh GREATER rh)
    set(ok 1)
  endif()
  set(${outvar} ${ok} PARENT_SCOPE)
endfunction()

dui_run(seed --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}"
  --exec "PLAYLIST_CREATE alpha" --exec "PLAYLIST_ADD_SONG 1 ${WORK}/a.mod" --exec "PLAYLIST_ADD_SONG 1 ${WORK}/b.mod" --exec "PLAYLIST_ADD_SONG 1 ${WORK}/c.mod")
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x800)
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(idle ${common} --input "${WORK}/idle.txt")
centre(side "${idle}" "sidelib_row 3 ")
file(WRITE "${WORK}/open.txt" "click ${side}\nwait 100\n")
dui_run(opened ${common} --input "${WORK}/open.txt")
row_of(r1 "${opened}" 1)
row_of(r2 "${opened}" 2)
centre(c1 "plsong 1 ${r1} " "plsong 1 ")
centre(c2 "plsong 2 ${r2} " "plsong 2 ")

# 1. One move onto row 1.
file(WRITE "${WORK}/m1.txt" "click ${side}\nwait 100\nmove ${c1}\nwait 100\n")
dui_run(m1 ${common} --input "${WORK}/m1.txt")
hover_of(h1 "${m1}")
hover_in(ok1 "${h1}" "${r1}")
if(NOT ok1)
  message(FATAL_ERROR "a pointer move onto playlist song row 1 (${r1}) did not hover it, hover_box ${h1}:\n${m1}")
endif()

# 2. The pointer crosses the sidebar and leaves the window between moves, then lands on row 2: hover_box follows to row 2.
#    Kept short: the desktop UI reads at most 4096 bytes of an --input script.
set(script "click ${side}\nwait 100\n")
foreach(i RANGE 1 10)
  string(APPEND script "move ${side}\nwait 20\nmove ${c1}\nwait 20\nmove 5 5\nwait 20\n")
endforeach()
string(APPEND script "move ${c2}\nwait 100\n")
file(WRITE "${WORK}/m2.txt" "${script}")
dui_run(m2 ${common} --input "${WORK}/m2.txt")
hover_of(h2 "${m2}")
hover_in(ok2 "${h2}" "${r2}")
if(NOT ok2)
  message(FATAL_ERROR "after moves over the sidebar a move onto song row 2 (${r2}) did not hover it, hover_box ${h2}:\n${m2}")
endif()
if(NOT "${m2}" MATCHES "hover_boxes n=([0-9]+) cap=([0-9]+)")
  message(FATAL_ERROR "no hover_boxes line in:\n${m2}")
endif()
if(NOT CMAKE_MATCH_1 LESS CMAKE_MATCH_2)
  message(FATAL_ERROR "the hover box list is full (n=${CMAKE_MATCH_1} of ${CMAKE_MATCH_2}): later rows are dropped:\n${m2}")
endif()

# 3. I1067 root cause: the window is resized twenty times and back, then the pointer moves onto row 1. Before the fix the hit-test
#    list kept the boxes of every old layout, filled to its cap, and the rows of the new layout were dropped: hover_box was a
#    stale box (or none) and the move repainted nothing. Now a whole-canvas repaint starts the list empty.
set(script "click ${side}\nwait 100\n")
foreach(i RANGE 1 20)
  math(EXPR rw "1280 + ${i} * 16")
  math(EXPR rh "800 + ${i} * 16")
  string(APPEND script "resize ${rw} ${rh}\nwait 50\n")
endforeach()
string(APPEND script "resize 1280 800\nwait 50\nmove 5 5\nwait 50\nmove ${c1}\nwait 100\n")
file(WRITE "${WORK}/m3.txt" "${script}")
dui_run(m3 ${common} --input "${WORK}/m3.txt")
hover_of(h3 "${m3}")
hover_in(ok3 "${h3}" "${r1}")
if(NOT "${m3}" MATCHES "hover_boxes n=([0-9]+) cap=([0-9]+)")
  message(FATAL_ERROR "no hover_boxes line after the resizes:\n${m3}")
endif()
if(NOT ok3 OR NOT CMAKE_MATCH_1 LESS CMAKE_MATCH_2)
  message(FATAL_ERROR "after twenty resizes a move onto song row 1 (${r1}) did not hover it: hover_box ${h3}, hover_boxes n=${CMAKE_MATCH_1} of ${CMAKE_MATCH_2}")
endif()
message("hover_motion_ok resize hover_box ${h3} n=${CMAKE_MATCH_1}")
