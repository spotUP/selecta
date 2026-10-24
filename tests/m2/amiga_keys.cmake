# I218 amiga-console-keys: the Amiga `Selecta` under vamos decodes Amiga console key bytes.
# console.device sends arrows as the 8-bit CSI 0x9B plus A/B/C/D, not ESC [ A. Down, Down, Up over
# four files leaves the browse cursor on row 1, and Shift+Down (0x9B S, no binding of its own, row
# I218b) then moves it like Down to row 2. The Latin-1 byte 0xE4 (a-umlaut, row I218a) is one key
# on the Amiga console, so the Down after it still arrives and leaves the cursor on row 3; a UTF-8
# decoder would swallow that Down's CSI byte. Sentinel view_cursor= and ui_frames=.
# The bytes go in through a keys file: a line that is no key name is fed byte by byte to the same
# decoder the terminal read feeds. Scripted stdin cannot carry this check, because all of stdin is
# decoded before the browse list is loaded.
# Inputs: VAMOS AMIGA_SELECTA WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(f a b c d)
  file(WRITE "${WORK}/songs/${f}.mod" "x")
endforeach()
string(ASCII 155 csi)
string(ASCII 228 auml)
file(WRITE "${WORK}/arrows.keys" "${csi}B\n${csi}B\n${csi}A\n${csi}S\n${auml}\n${csi}B\nq\n")
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -V work:${WORK} -- "${AMIGA_SELECTA}" --size 80x25
    --start-dir work:songs --keys work:arrows.keys
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[I218]: Selecta under vamos exited ${rc}\n${out}${err}")
endif()
m2_check(I218 "${out}${err}" "ui_frames>=1,view_cursor==3")
message(STATUS "[OK] I218")

# I218c: a window-resized event then the bounds report (rows 50, cols 120) reach the same decoder;
# the text UI regrows its grid and tells the engine the new list height: 80x25 gives 21 list rows,
# 120x50 gives 46. Sentinel view_window= (the engine's requested window height).
file(WRITE "${WORK}/resize.keys" "${csi}12;0;0;0;0;0;0;0|\n${csi}1;1;50;120 r\nq\n")
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -V work:${WORK} -- "${AMIGA_SELECTA}" --size 80x25
    --start-dir work:songs --keys work:resize.keys
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[I218c]: Selecta under vamos exited ${rc}\n${out}${err}")
endif()
m2_check(I218c "${out}${err}" "view_window==46")
message(STATUS "[OK] I218c")
