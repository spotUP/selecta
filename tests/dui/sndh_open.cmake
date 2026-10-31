# sndh-open (I427b part 2): a SNDH dropped on the desktop app plays through the GPL sc68 module.
# -DSELECTA -DSONG -DMODDIR -DEMPTY -DWORK. Same drive as dui-drop: `drop <file>` is the desktop's open path.
# 1. Module on $SELECTA_PLUGIN_PATH: songs_opened=1, replayer=sc68, the loader counted one dynamic module, the engine
#    called RP_RENDER (replayer_render_calls >= 1, rendered_frames > 0) and the null output pulled a non-silent sample
#    (out_peak >= 0x0800, about -24 dBFS, same floor as sndh-render). No open-fail toast.
# 2. Empty module directory: the open fails, open_fail_toasts=1 and the toast reads "No player for that file type yet"
#    (a dropped path has no source prefix; the library-key toast "No player for SNDH yet" is open-fail-toast's).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
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

sndh_run(with "${MODDIR}")
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
if(NOT without MATCHES "open_fail_toasts=1 open_fail_text=\"No player for that file type yet\"")
  message(FATAL_ERROR "without the module the open must raise the no-player toast:\n${without}")
endif()
if(without MATCHES "replayer=sc68")
  message(FATAL_ERROR "sc68 rendered without its module on the plugin path:\n${without}")
endif()
