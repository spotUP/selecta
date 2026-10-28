# dui-quit-dialog (I623): Escape with nothing open asks "Quit Selecta?" before it quits.
#   -DSELECTA -DWORK
#   Violated invariant: Escape did nothing on a bare page, so the desktop UI could not be left from the keyboard. Escape with
#   nothing open now opens the confirm dialog of DUI-P4-18 in its Quit kind (dialog open=3, opens counts it); Escape or
#   Cancel closes it and the UI keeps running; Enter or the Quit button sets quits=1 and ends the loop (the script lines after
#   it never run, the process exits 0). Escape with an overlay open closes the overlay only and opens no dialog.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got ${got}, want ${want}")
  endif()
endfunction()

function(run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --ui desktop --backend mem --canvas 900x600 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(field outvar out name)
  if(NOT "${out}" MATCHES "dialog [^\n]*${name}=([0-9]+)")
    message(FATAL_ERROR "no dialog ${name} in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(centre xv yv out key)
  if(NOT "${out}" MATCHES "dialog [^\n]*${key}=([0-9]+),([0-9]+),([0-9]+),([0-9]+)")
    message(FATAL_ERROR "no dialog ${key} in:\n${out}")
  endif()
  math(EXPR x "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${xv} ${x} PARENT_SCOPE)
  set(${yv} ${y} PARENT_SCOPE)
endfunction()

# 1. Escape with nothing open opens the quit dialog (sentinel: open=3, opens=1); nothing quit yet.
run(a "wait 1\nkey escape\n")
field(v "${a}" open)
expect_eq("quit dialog kind" "${v}" 3)
field(v "${a}" opens)
expect_eq("dialogs opened" "${v}" 1)
field(v "${a}" quits)
expect_eq("quit before the answer" "${v}" 0)

# 2. Escape in the dialog cancels; the UI keeps running (a second Escape opens it again: opens=2).
run(b "wait 1\nkey escape\nkey escape\n")
field(v "${b}" open)
expect_eq("dialog after Escape" "${v}" 0)
field(v "${b}" cancels)
expect_eq("cancels" "${v}" 1)
field(v "${b}" quits)
expect_eq("quit after Cancel" "${v}" 0)
run(b2 "wait 1\nkey escape\nkey escape\nkey escape\n")
field(v "${b2}" opens)
expect_eq("UI still running after Escape in the dialog" "${v}" 2)

# 3. The Cancel button closes it too.
centre(cx cy "${a}" cancel)
run(c "wait 1\nkey escape\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nkey escape\n")
field(v "${c}" cancels)
expect_eq("cancels via the button" "${v}" 1)
field(v "${c}" opens)
expect_eq("UI still running after Cancel" "${v}" 2)

# 4. Enter confirms: quits=1 and the loop ended, so the Escape after it never ran (opens stays 1). rc 0 is checked by dui_run.
run(d "wait 1\nkey escape\nkey enter\nkey escape\n")
field(v "${d}" quits)
expect_eq("quit after Enter" "${v}" 1)
field(v "${d}" confirms)
expect_eq("confirms" "${v}" 1)
field(v "${d}" opens)
expect_eq("script ran on after Quit" "${v}" 1)

# 5. The Quit button confirms the same way.
centre(qx qy "${a}" confirm)
run(e "wait 1\nkey escape\nmove ${qx} ${qy}\nclick ${qx} ${qy}\nkey escape\n")
field(v "${e}" quits)
expect_eq("quit after the button" "${v}" 1)
field(v "${e}" opens)
expect_eq("script ran on after the Quit button" "${v}" 1)

# 6. Escape with an overlay open closes the overlay only: no dialog.
run(f "wait 1\nkey ctrl+p\nkey escape\n")
field(v "${f}" opens)
expect_eq("dialog opened over an overlay" "${v}" 0)
field(v "${f}" quits)
expect_eq("quit over an overlay" "${v}" 0)
if(NOT "${f}" MATCHES "overlay kind=0 palette_opens=1 ")
  message(FATAL_ERROR "Escape did not close the palette (opened once, kind 0 expected):\n${f}")
endif()
