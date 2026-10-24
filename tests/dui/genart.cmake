# dui-genart (DUI-P2-5): generated artwork for songs without a cover. -DSELECTA -DSONGS -DWORK -DGOLDEN
# No library is open, so no title has a cover. The first home tile of a song named "Tune" then shows the
# generated art of its format; STAT_ART_TILE_HASH is the hash of that tile square.
# 1. The mod and the xm tile hashes match their goldens (update only with SELECTA_GOLDEN_UPDATE=1).
# 2. The two formats (different chips: paula and software) give different hashes.
# 3. Two runs of one format give the same hash (the art is a pure function of format and title).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")

function(tile_hash outvar ext)
  dui_run(out --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD \"fixture/Tune.${ext}\"")
  dui_stat(h "${out}" desktop art_tile_hash)
  set(${outvar} "${h}" PARENT_SCOPE)
endfunction()

foreach(ext mod xm)
  tile_hash(h_${ext} ${ext})
  tile_hash(again_${ext} ${ext})
  if(NOT h_${ext} STREQUAL again_${ext})
    message(FATAL_ERROR "genart ${ext} is not deterministic: ${h_${ext}} vs ${again_${ext}}")
  endif()
  set(gfile "${GOLDEN}/desktop/genart-${ext}.hash")
  if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
    file(WRITE "${gfile}" "${h_${ext}}\n")
  endif()
  if(NOT EXISTS "${gfile}")
    message(FATAL_ERROR "missing golden ${gfile} (hash ${h_${ext}}); create it with SELECTA_GOLDEN_UPDATE=1")
  endif()
  file(STRINGS "${gfile}" want LIMIT_COUNT 1)
  if(NOT h_${ext} STREQUAL want)
    message(FATAL_ERROR "genart ${ext} hash ${h_${ext}} differs from golden ${want} (${gfile})")
  endif()
endforeach()
if(h_mod STREQUAL h_xm)
  message(FATAL_ERROR "mod and xm must give different generated art (both ${h_mod})")
endif()
