# dui-cover-tiles (I692): the Now Playing bar, the Now Playing panel and the sidebar album rows draw the real cover. -DSELECTA -DSONGS -DWORK
#   Violated invariant: those three sites drew sel_gui_genart for every song and album; only the album grid and the queue cards blitted art_tile.
#   A seed run scans the art-music fixture (its albums carry covers). The second run queues a tagged song and plays it: the dump line
#   `cover_tiles np= bar= side=` counts the draws that blitted a cover, so the product entry point (the window) must show bar >= 1 and np >= 1,
#   and the sidebar (Albums chip shown by default with the fixture library) side >= 1. the counters move only when art_tile answered with pixels (the seed run already wrote the thumb rows, so no decode is expected in this run).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(music "${HERE}/fixtures/art-music")
set(a "${music}/Aurora Fields/Glass Rivers/01 - Glass Rivers Track 1.mp3")
set(common --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${WORK}/library.db")
file(WRITE "${WORK}/seed.txt" "wait 1\nexec LIBRARY_SCAN ${music}\nwait 8\n")
dui_run(seed ${common} --input "${WORK}/seed.txt")
file(WRITE "${WORK}/play.txt" "exec QUEUE_ADD \"${a}\"\nexec PLAY\nwait 5\n")
dui_run(o ${common} --input "${WORK}/play.txt")
if(NOT "${o}" MATCHES "cover_tiles np=([0-9]+) bar=([0-9]+) side=([0-9]+)\n")
  message(FATAL_ERROR "no cover_tiles line in:\n${o}")
endif()
set(np ${CMAKE_MATCH_1})
set(bar ${CMAKE_MATCH_2})
set(side ${CMAKE_MATCH_3})
if(np LESS 1 OR bar LESS 1 OR side LESS 1)
  message(FATAL_ERROR "np=${np} bar=${bar} side=${side}: a cover site drew generated art\n${o}")
endif()
