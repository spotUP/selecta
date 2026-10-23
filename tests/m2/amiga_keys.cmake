# I218 amiga-console-keys: the Amiga `Selecta` under vamos decodes Amiga console key bytes.
# console.device sends arrows as the 8-bit CSI 0x9B plus A/B/C/D, not ESC [ A. Down, Down, Up over
# three files leaves the browse cursor on row 1; sentinel view_cursor= and ui_frames=.
# The bytes go in through a keys file: a line that is no key name is fed byte by byte to the same
# decoder the terminal read feeds. Scripted stdin cannot carry this check, because all of stdin is
# decoded before the browse list is loaded.
# Inputs: VAMOS AMIGA_SELECTA WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(f a b c)
  file(WRITE "${WORK}/songs/${f}.mod" "x")
endforeach()
string(ASCII 155 csi)
file(WRITE "${WORK}/arrows.keys" "${csi}B\n${csi}B\n${csi}A\nq\n")
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -V work:${WORK} -- "${AMIGA_SELECTA}" --size 80x25
    --start-dir work:songs --keys work:arrows.keys
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[I218]: Selecta under vamos exited ${rc}\n${out}${err}")
endif()
m2_check(I218 "${out}${err}" "ui_frames>=1,view_cursor==1")
message(STATUS "[OK] I218")
