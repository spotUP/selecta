# T55 amiga-text-ui-vamos: the Amiga `Selecta` binary under vamos with scripted stdin (`q`), one 80x25
# xterm-256color frame; sentinel ui_frames=; the byte stream hash (ui_hash=) equals the desktop text UI frame.
# Both runs pass an empty --start-dir: no library browse, so the frame does not depend on the working directory.
# Inputs: VAMOS AMIGA_SELECTA DESKTOP_SELECTA KEYS
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
if(NOT EXISTS "${AMIGA_SELECTA}")
  message(FATAL_ERROR "RED[T55]: ${AMIGA_SELECTA} not built: apps/selecta has no Amiga build (plugins/ui/text/amiga/term.c missing).")
endif()
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -- "${AMIGA_SELECTA}" --size 80x25 --start-dir ""
  INPUT_FILE "${KEYS}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[T55]: Selecta under vamos exited ${rc}\n${out}${err}")
endif()
m2_check(T55 "${out}${err}" "ui_frames>=1")
m2_get("${out}${err}" ui_hash mine)
execute_process(COMMAND "${DESKTOP_SELECTA}" --output null --size 80x25 --start-dir ""
  INPUT_FILE "${KEYS}" OUTPUT_VARIABLE dout ERROR_VARIABLE derr RESULT_VARIABLE drc)
m2_get("${dout}${derr}" ui_hash theirs)
if(NOT mine STREQUAL theirs)
  message(FATAL_ERROR "RED[T55]: ui_hash 68k=${mine} desktop=${theirs}")
endif()
message(STATUS "[OK] T55")
