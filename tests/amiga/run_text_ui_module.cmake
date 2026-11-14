# I1378b (design 2026-11-13-amiga-plugins-on-demand, "Design" item 1 and 4). The text UI is the LoadSeg module
# Players/sel_text_ui; the executable links only its manifest. Runs the 68k selecta-cli under vamos with PROGDIR: on a
# staged drawer (the run_seg_imports.cmake pattern):
#   MODE=present  Players/ holds sel_text_ui and nothing else: `--ui text` opens, draws a frame and quits on `q`;
#                 module_loads==1 proves the UI came from the module, and no other module was needed.
#   MODE=missing  Players/ is empty: the start fails (exit 2) and the message names Players/sel_text_ui.
# Revert proof: run MODE=present with the module left out of the drawer (MODE=present-empty): it must fail.
# Inputs: VAMOS, AMIGA_SELECTA (the 68k selecta, not selecta-cli: only it takes --ui), MODULE (build/amiga/Players/sel_text_ui), KEYS, WORK, MODE.
set(MODE vamos)
include("${CMAKE_CURRENT_LIST_DIR}/../m2/lib.cmake")
set(WANT "${TEST_MODE}")
set(TAG I1378b)

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/drawer/Players")
if(NOT EXISTS "${MODULE}")
  message(FATAL_ERROR "RED[${TAG}]: ${MODULE} missing (build target sel_seg_text_ui).")
endif()
if(WANT STREQUAL "present" AND NOT SKIP_COPY) # SKIP_COPY: the revert proof, the module is left out of the drawer
  configure_file("${MODULE}" "${WORK}/drawer/Players/sel_text_ui" COPYONLY)
endif()

sel_vamos_cmd(vcmd "${AMIGA_SELECTA}")
list(FIND vcmd "PROGDIR:selbin:" i)
if(i LESS 0)
  message(FATAL_ERROR "RED[${TAG}]: sel_vamos_cmd no longer assigns PROGDIR:selbin: (tests/amiga/vamos_cmd.cmake changed).")
endif()
list(REMOVE_AT vcmd ${i})
list(INSERT vcmd ${i} "PROGDIR:segtest:")
execute_process(COMMAND ${vcmd} -V "segtest:${WORK}/drawer" -V "work:${WORK}"
    -- "${AMIGA_SELECTA}" --ui text --output null --size 80x25 --start-dir "" --db work:amiga.db
  INPUT_FILE "${KEYS}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 90)
if(WANT STREQUAL "present")
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "RED[${TAG}]: --ui text with only Players/sel_text_ui exited ${rc}\n${out}${err}")
  endif()
  m2_check(${TAG} "${out}${err}" "ui_frames>=1,module_loads==1")
else()
  if(rc EQUAL 0)
    message(FATAL_ERROR "RED[${TAG}]: --ui text started although Players/sel_text_ui is missing\n${out}${err}")
  endif()
  if(NOT "${out}${err}" MATCHES "Players/sel_text_ui")
    message(FATAL_ERROR "RED[${TAG}]: the start-up error does not name Players/sel_text_ui\n${out}${err}")
  endif()
endif()
message(STATUS "[OK] ${TAG} ${WANT}")
