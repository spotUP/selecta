# dui-artwork-late-album (I689): a missed art lookup must not outlive a library change. -DSELECTA -DSONGS -DWORK
#   Violated invariant: art.c cached album 0 (and the bad list) for a title for the whole process, so songs dropped while the app ran
#   (the song plays before the scan has created its albums row) kept the generated art until a relaunch. The window queues and plays a
#   tagged mp3 that is not in the library yet (find answers 0, the bar draws generated art), then `exec LIBRARY_SCAN` adds it; the library
#   serial moves, desk_library_follow calls art_forget_misses, and the next frame resolves the album and decodes the cover.
#   art_decodes (STAT_ART_DECODES) must be >= 1 and the bar must have blitted the cover; with art_forget_misses removed both stay 0.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(music "${HERE}/fixtures/art-music")
set(a "${music}/Aurora Fields/Glass Rivers/01 - Glass Rivers Track 1.mp3")
file(WRITE "${WORK}/in.txt" "exec QUEUE_ADD \"${a}\"\nexec PLAY\nwait 3\nexec LIBRARY_SCAN ${music}\nwait 8\n")
dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
  --input "${WORK}/in.txt")
dui_stat(decodes "${o}" desktop art_decodes)
if(NOT "${o}" MATCHES "cover_tiles np=([0-9]+) bar=([0-9]+) side=([0-9]+)\n")
  message(FATAL_ERROR "no cover_tiles line in:\n${o}")
endif()
set(bar ${CMAKE_MATCH_2})
if(decodes LESS 1 OR bar LESS 1)
  message(FATAL_ERROR "decodes=${decodes} bar=${bar}: a song played before its album existed never got its cover after the scan:\n${o}")
endif()
