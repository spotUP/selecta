# dui-default-ui (I1023): a launch with no --ui and no --backend (what a double-click on Selecta.app does) opens the desktop UI
# drawing through the sdl backend. -DSELECTA -DWORK -DSDL_BUILT
# Runs the bundle binary headless (SDL_VIDEODRIVER=dummy): the `ui_backend` line of --stats names the UI that ran and its backend.
# On macOS only; every other host keeps the text default, which the registry unit test (default-output-unit) covers.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
dui_require_sdl()
if(NOT APPLE)
  message("SELECTA_SKIP: the desktop default UI is the macOS machine default (I1023)")
  return()
endif()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{SDL_VIDEODRIVER} dummy)
file(WRITE "${WORK}/quit.txt" "resize 960 600\n")
# No --ui, no --backend, no scripted-run marker (--size, --output, --keys): what a double-click does, plus the desktop's own
# test flags. dui_run is not used: it adds --output null --size, which are exactly the markers that keep the text default.
execute_process(
  COMMAND "${SELECTA}" --config-dir "${WORK}/cfg" --canvas 1280x800 --clock fixed --stats --input "${WORK}/quit.txt"
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc INPUT_FILE /dev/null TIMEOUT 60)
if(NOT out MATCHES "ui_backend id=desktop backend=sdl ")
  message(FATAL_ERROR "a launch with no --ui must open the desktop UI on the sdl backend; got:\n${out}")
endif()
dui_stat(presents "${out}" desktop presents)
if(presents EQUAL 0)
  message(FATAL_ERROR "the sdl backend never presented a frame")
endif()
# The binary is the app bundle's executable.
if(NOT SELECTA MATCHES "Selecta\\.app/Contents/MacOS/Selecta$")
  message(FATAL_ERROR "selecta is not the Selecta.app bundle executable: ${SELECTA}")
endif()
get_filename_component(MACOS "${SELECTA}" DIRECTORY)
if(NOT EXISTS "${MACOS}/../Info.plist" OR NOT EXISTS "${MACOS}/../Resources/Selecta.icns")
  message(FATAL_ERROR "bundle lacks Info.plist or Resources/Selecta.icns under ${MACOS}/..")
endif()

# A scripted run (--keys, --size, --output null) with no --ui keeps the text UI and opens no desktop window.
file(WRITE "${WORK}/q.keys" "q")
execute_process(
  COMMAND "${SELECTA}" --output null --size 80x25 --config-dir "${WORK}/cfg2" --keys "${WORK}/q.keys" --stats
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE sout ERROR_VARIABLE serr RESULT_VARIABLE src INPUT_FILE /dev/null TIMEOUT 60)
if(NOT sout MATCHES "ui_backend id=text " OR sout MATCHES "id=desktop")
  message(FATAL_ERROR "a scripted run with no --ui must run the text UI and open no desktop window; got:\n${sout}\n${serr}")
endif()
