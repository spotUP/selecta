# GPL-3 module archive (I427d, design 2026-10-30-plugin-loader-sdk D5). Script mode, run by the selecta-gpl-sndh-archive
# target: -DMODULE=<sel_sndh dylib/so> -DSOURCE_DIR=<third_party/sc68> -DOUT=<archive.tar.gz> -DSTAGE=<scratch dir>.
# The archive holds the module, the GPL-3 COPYING text and the complete vendored sc68 source (with VERSION.txt and
# PATCHES.txt), so the module is never shipped without the licence and the corresponding source. It is separate from the
# MIT release archive and the app bundle.
foreach(v MODULE SOURCE_DIR OUT STAGE)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "gpl_archive.cmake: -D${v} missing")
  endif()
endforeach()
foreach(f "${MODULE}" "${SOURCE_DIR}/COPYING" "${SOURCE_DIR}/VERSION.txt" "${SOURCE_DIR}/PATCHES.txt")
  if(NOT EXISTS "${f}")
    message(FATAL_ERROR "gpl_archive.cmake: ${f} not found")
  endif()
endforeach()
set(root "${STAGE}/selecta-sndh-gpl3")
file(REMOVE_RECURSE "${STAGE}")
file(MAKE_DIRECTORY "${root}/PlugIns")
file(COPY "${MODULE}" DESTINATION "${root}/PlugIns")
file(COPY "${SOURCE_DIR}/COPYING" DESTINATION "${root}")
file(COPY "${SOURCE_DIR}" DESTINATION "${root}/source" PATTERN ".git" EXCLUDE)
get_filename_component(srcname "${SOURCE_DIR}" NAME)
file(WRITE "${root}/README.txt"
  "Selecta SNDH module (sc68), licensed GPL-3. See COPYING.\n"
  "Copy PlugIns/ contents into Selecta.app/Contents/PlugIns or into a folder on SELECTA_PLUGIN_PATH.\n"
  "The complete corresponding source of the sc68 code in the module is in source/${srcname}/.\n")
get_filename_component(outdir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${outdir}")
file(REMOVE "${OUT}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${OUT}" selecta-sndh-gpl3
  WORKING_DIRECTORY "${STAGE}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "gpl_archive.cmake: tar failed (${rc})")
endif()
file(REMOVE_RECURSE "${STAGE}")
message(STATUS "gpl_archive_ok ${OUT}")
