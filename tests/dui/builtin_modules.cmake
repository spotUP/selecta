# builtin-modules (I427f, design 2026-10-30-plugin-loader-sdk D6): with SELECTA_PLUGIN_LINK=MODULE the desktop app links
# no plugin and loads every built-in as a module. -DSELECTA -DNM -DSONG -DMODDIR -DWORK.
# 1. nm on the selecta binary lists no sel_plugin_* symbol (the plugins are not in the executable).
# 2. Modules from MODDIR (SELECTA_BUILTIN_MODULES_DIR): the song opens, a module replayer renders it
#    (replayer_render_calls >= 1, rendered_frames > 0), no module is reported missing.
# 3. A MODDIR copy without sel_nowplaying.dylib/.so: the app starts, and the missing module is reported through the
#    toast path (module_missing toasts=1, the text names the file).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")

execute_process(COMMAND "${NM}" -g "${SELECTA}" OUTPUT_VARIABLE syms RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "nm failed on ${SELECTA}")
endif()
string(REGEX MATCHALL "[^\n]*sel_plugin_[^\n]*" hits "${syms}")
list(LENGTH hits nhits)
if(NOT nhits EQUAL 0)
  message(FATAL_ERROR "the selecta binary still links ${nhits} sel_plugin_* symbols:\n${hits}")
endif()
message(STATUS "nm evidence: ${SELECTA} defines 0 sel_plugin_* symbols")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
file(COPY_FILE "${SONG}" "${WORK}/songs/test.nsf")

function(bm_run outvar mods)
  set(ENV{SELECTA_BUILTIN_MODULES_DIR} "${mods}")
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(REMOVE "${WORK}/library.db")
  file(WRITE "${WORK}/in.txt" "wait 1\ndrop ${WORK}/songs/test.nsf\nwait 40\n")
  dui_run(o --start-dir "${WORK}/songs" --db "${WORK}/library.db" --ui desktop --backend mem --view queue --canvas 900x600
    --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

bm_run(with "${MODDIR}")
if(NOT with MATCHES "songs_opened=1 ")
  message(FATAL_ERROR "the dropped song was not opened with the built-in modules:\n${with}")
endif()
if(NOT with MATCHES "replayer=gme")
  message(FATAL_ERROR "the song was not rendered by the gme module:\n${with}")
endif()
if(NOT with MATCHES "replayer_render_calls=([1-9][0-9]*) rendered_frames=([1-9][0-9]*)")
  message(FATAL_ERROR "no module replayer rendered (replayer_render_calls / rendered_frames):\n${with}")
endif()
if(NOT with MATCHES "module_missing toasts=0 ")
  message(FATAL_ERROR "a built-in module was reported missing with the full set present:\n${with}")
endif()

file(GLOB all_mods "${MODDIR}/sel_*")
file(MAKE_DIRECTORY "${WORK}/mods")
foreach(m IN LISTS all_mods)
  get_filename_component(n "${m}" NAME)
  if(NOT n MATCHES "^sel_nowplaying\\.")
    file(COPY "${m}" DESTINATION "${WORK}/mods")
  endif()
endforeach()
bm_run(without "${WORK}/mods")
if(NOT without MATCHES "module_missing toasts=1 text=\"Plugin module missing: sel_nowplaying\\.")
  message(FATAL_ERROR "a missing built-in module must raise exactly one error toast:\n${without}")
endif()
