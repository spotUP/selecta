# bundle-plugins (I427d, design 2026-10-30-plugin-loader-sdk D5): the macOS bundle starts with its modules from
# Contents/PlugIns, and GPL modules ship in their own archive.
# -DSELECTA=<Selecta.app/Contents/MacOS/Selecta> -DSONG -DWORK -DGPL_SCRIPT=<cmake/gpl_archive.cmake>
# optional (SELECTA_GPL_PLUGINS=ON): -DGPL_MODULE -DGPL_SOURCE -DGPL_ARCHIVE -DNM_TAR=1
# 1. The bundled binary runs with SELECTA_BUILTIN_MODULES_DIR and SELECTA_PLUGIN_PATH unset: the song opens and the
#    replayer_origin sentinel (the module path the open song's replayer came from) lies in <bundle>/Contents/PlugIns.
# 2. The bundle's PlugIns holds no GPL module (sel_sndh).
# 3. The GPL archive script builds a tar.gz that lists COPYING, the module and the sc68 source files.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")

get_filename_component(macos "${SELECTA}" DIRECTORY)
get_filename_component(contents "${macos}" DIRECTORY)
get_filename_component(contents "${contents}" REALPATH)
if(NOT contents MATCHES "/Contents$" OR NOT IS_DIRECTORY "${contents}/PlugIns")
  message(FATAL_ERROR "${SELECTA} is not inside a bundle with Contents/PlugIns (got ${contents})")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
file(COPY_FILE "${SONG}" "${WORK}/songs/test.nsf")
unset(ENV{SELECTA_BUILTIN_MODULES_DIR})
unset(ENV{SELECTA_PLUGIN_PATH})
file(WRITE "${WORK}/in.txt" "wait 1\ndrop ${WORK}/songs/test.nsf\nwait 40\n")
dui_run(o --start-dir "${WORK}/songs" --db "${WORK}/library.db" --ui desktop --backend mem --view queue --canvas 900x600
  --clock fixed --stats --input "${WORK}/in.txt")
if(NOT o MATCHES "songs_opened=1 " OR NOT o MATCHES "replayer_render_calls=([1-9][0-9]*) ")
  message(FATAL_ERROR "the bundled app did not open and render the song:\n${o}")
endif()
if(NOT o MATCHES "replayer_origin=([^\n]*)\n")
  message(FATAL_ERROR "no replayer_origin line:\n${o}")
endif()
set(origin "${CMAKE_MATCH_1}")
get_filename_component(origin_real "${origin}" REALPATH)
if(NOT origin_real MATCHES "^${contents}/PlugIns/sel_[a-z0-9_]+\\.(dylib|so)$")
  message(FATAL_ERROR "the replayer module did not come from ${contents}/PlugIns: ${origin}")
endif()
message(STATUS "sentinel: replayer module loaded from ${origin}")

file(GLOB gpl_in_bundle "${contents}/PlugIns/sel_sndh.*" "${contents}/PlugIns/sel_sid.*")
if(gpl_in_bundle)
  message(FATAL_ERROR "GPL module(s) inside the MIT bundle: ${gpl_in_bundle}")
endif()

if(GPL_ARCHIVE)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DMODULE=${GPL_MODULE}" "-DSOURCE_DIR=${GPL_SOURCE}" "-DOUT=${GPL_ARCHIVE}"
    "-DSTAGE=${WORK}/stage" -P "${GPL_SCRIPT}" RESULT_VARIABLE rc OUTPUT_VARIABLE aout ERROR_VARIABLE aerr)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "gpl archive script failed:\n${aout}\n${aerr}")
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tzf "${GPL_ARCHIVE}" OUTPUT_VARIABLE listing RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "cannot list ${GPL_ARCHIVE}")
  endif()
  foreach(want "selecta-sndh-gpl3/COPYING" "selecta-sndh-gpl3/source/sc68/COPYING" "selecta-sndh-gpl3/source/sc68/VERSION.txt"
      "selecta-sndh-gpl3/source/sc68/PATCHES.txt" "selecta-sndh-gpl3/source/sc68/libsc68/" "selecta-sndh-gpl3/PlugIns/sel_sndh.")
    string(FIND "${listing}" "${want}" pos)
    if(pos LESS 0)
      message(FATAL_ERROR "GPL archive lacks ${want}:\n${listing}")
    endif()
  endforeach()
  string(REGEX MATCHALL "[^\n]*\\.[ch]\n" srcs "${listing}")
  list(LENGTH srcs nsrc)
  if(nsrc LESS 20)
    message(FATAL_ERROR "GPL archive holds only ${nsrc} C source files")
  endif()
  message(STATUS "gpl archive lists COPYING, the module and ${nsrc} sc68 C files")
endif()
message(STATUS "bundle_plugins_ok")
