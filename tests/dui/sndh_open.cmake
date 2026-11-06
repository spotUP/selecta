# sndh-open (I427b part 2): a SNDH dropped on the desktop app plays through the GPL sc68 module.
# -DSELECTA -DSONG -DMODDIR -DEMPTY -DWORK. Same drive as dui-drop: `drop <file>` is the desktop's open path.
# 1. Module on $SELECTA_PLUGIN_PATH: songs_opened=1, replayer=sc68, the loader counted one dynamic module, the engine
#    called RP_RENDER (replayer_render_calls >= 1, rendered_frames > 0) and the null output pulled a non-silent sample
#    (out_peak >= 0x0800, about -24 dBFS, same floor as sndh-render). No open-fail toast.
# 2. Empty module directory: sc68 is absent, the bundled psgplay (I707) plays the tune: songs_opened=1, replayer=psgplay,
#    no failure toast, no sc68.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")

# The module under test is the ONLY way sc68 reaches the app. The shipped bundle carries sel_sndh in its own plugin folder
# (Contents/PlugIns, else <exe dir>/plugins), which the loader scans after $SELECTA_PLUGIN_PATH, so an empty
# $SELECTA_PLUGIN_PATH alone never removes it. Run a symlink of the executable inside an isolated app layout whose plugin
# folder holds every shipped module except sel_sndh.*; $SELECTA_PLUGIN_PATH then carries sel_sndh (or nothing).
get_filename_component(EXE_DIR "${SELECTA}" DIRECTORY)
get_filename_component(EXE_NAME "${SELECTA}" NAME)
if(EXE_DIR MATCHES "/MacOS$")
  set(REAL_PLUGINS "${EXE_DIR}/../PlugIns")
  set(ISO_EXE_DIR "${WORK}/app/Contents/MacOS")
  set(ISO_PLUGINS "${WORK}/app/Contents/PlugIns")
else()
  set(REAL_PLUGINS "${EXE_DIR}/plugins")
  set(ISO_EXE_DIR "${WORK}/app/bin")
  set(ISO_PLUGINS "${WORK}/app/bin/plugins")
endif()
file(MAKE_DIRECTORY "${ISO_EXE_DIR}" "${ISO_PLUGINS}" "${WORK}/sndh_only")
file(CREATE_LINK "${SELECTA}" "${ISO_EXE_DIR}/${EXE_NAME}" SYMBOLIC)
file(GLOB shipped "${REAL_PLUGINS}/*")
foreach(m IN LISTS shipped)
  get_filename_component(n "${m}" NAME)
  if(NOT n MATCHES "^sel_sndh\\.")
    file(CREATE_LINK "${m}" "${ISO_PLUGINS}/${n}" SYMBOLIC)
  endif()
endforeach()
file(GLOB sndh_mod "${MODDIR}/sel_sndh.*")
if(NOT sndh_mod)
  message(FATAL_ERROR "no sel_sndh module in ${MODDIR}")
endif()
foreach(m IN LISTS sndh_mod)
  get_filename_component(n "${m}" NAME)
  file(CREATE_LINK "${m}" "${WORK}/sndh_only/${n}" SYMBOLIC)
endforeach()
set(SELECTA "${ISO_EXE_DIR}/${EXE_NAME}")
file(COPY_FILE "${SONG}" "${WORK}/songs/oculus.sndh")

function(sndh_run outvar moddir)
  set(ENV{SELECTA_PLUGIN_PATH} "${moddir}")
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(REMOVE "${WORK}/library.db")
  file(WRITE "${WORK}/in.txt" "wait 1\ndrop ${WORK}/songs/oculus.sndh\nwait 40\n")
  dui_run(o --start-dir "${WORK}/songs" --db "${WORK}/library.db" --ui desktop --backend mem --view queue --canvas 900x600
    --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

sndh_run(with "${WORK}/sndh_only")
if(NOT with MATCHES "replayer=sc68")
  message(FATAL_ERROR "the open song was not rendered by sc68:\n${with}")
endif()
if(NOT with MATCHES "songs_opened=1 ")
  message(FATAL_ERROR "the dropped SNDH was not opened:\n${with}")
endif()
if(NOT with MATCHES "replayer_render_calls=([1-9][0-9]*) rendered_frames=([1-9][0-9]*) out_peak=([0-9]+)")
  message(FATAL_ERROR "sc68 rendered nothing (replayer_render_calls / rendered_frames):\n${with}")
endif()
if(CMAKE_MATCH_3 LESS 2048)
  message(FATAL_ERROR "the output is silent: out_peak=${CMAKE_MATCH_3}, want at least 2048 (0x0800)\n${with}")
endif()
if(NOT with MATCHES "open_fail_toasts=0 ")
  message(FATAL_ERROR "the open raised a failure toast:\n${with}")
endif()

sndh_run(without "${EMPTY}")
# I707: psgplay is the second SNDH backend and is bundled, so without sc68 the open still succeeds, through psgplay.
if(NOT without MATCHES "songs_opened=1 ")
  message(FATAL_ERROR "without sc68 the dropped SNDH was not opened (psgplay should play it):\n${without}")
endif()
if(NOT without MATCHES "replayer=psgplay")
  message(FATAL_ERROR "without sc68 the open must run through replayer=psgplay:\n${without}")
endif()
if(without MATCHES "replayer=sc68")
  message(FATAL_ERROR "sc68 rendered without its module on the plugin path:\n${without}")
endif()
if(NOT without MATCHES "open_fail_toasts=0 ")
  message(FATAL_ERROR "the psgplay open raised a failure toast:\n${without}")
endif()
