# dui-platform-covers (I691b): a song without real art shows the platform cover of its file name's family. -DSELECTA -DWORK
#   Design: thoughts/shared/research/2026-10-29-i691-platform-covers.md sections 3 and 4. Violated invariant: the song sites drew the
#   anonymous genart gradient. The dump line `platform_covers np= bar= page= tile0= draws=` names the family (lower case id) each site
#   drew, `none` when real art won; draws counts the fallback branch (the sentinel: it must move, so the cover code RAN).
#   1. Home queue tiles: mod -> amiga, sid -> commodore_64, xm -> pc_tracker, spc -> snes (the family table, one row per ext), mp3 -> music.
#   2. `mod.Tune` (Amiga prefix) -> amiga.
#   3. A playing mod: the Now Playing panel and the player bar (home) and the Now Playing page draw amiga.
#   4. A song with real art (art-music fixture, mp3) draws no platform cover at np and bar: `none` (cover_tiles still holds).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(fx "${HERE}/../m3/lyrics")

function(pc_line outvar out)
  if(NOT "${out}" MATCHES "platform_covers np=([a-z0-9_]+) bar=([a-z0-9_]+) page=([a-z0-9_]+) tile0=([a-z0-9_]+) draws=([0-9]+)\n")
    message(FATAL_ERROR "no platform_covers line in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4} ${CMAKE_MATCH_5}" PARENT_SCOPE)
endfunction()

# 1 and 2: the first home tile of a queued (not playing) song.
function(tile_family name expect)
  dui_run(out --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD \"fixture/${name}\"")
  pc_line(l "${out}")
  separate_arguments(l)
  list(GET l 3 tile0)
  list(GET l 4 draws)
  if(NOT tile0 STREQUAL "${expect}")
    message(FATAL_ERROR "${name}: tile0=${tile0}, want ${expect}\n${out}")
  endif()
  if(draws LESS 1)
    message(FATAL_ERROR "${name}: draws=${draws}: the platform cover never ran\n${out}")
  endif()
endfunction()
tile_family(Tune.mod amiga)
tile_family(Tune.sid commodore_64)
tile_family(Tune.xm pc_tracker)
tile_family(Tune.spc snes)
tile_family(Tune.mp3 music)
tile_family(mod.Tune amiga)

# 3: a playing mod, home view (panel and bar) and the Now Playing page.
dui_run(oh --db "${WORK}/h.db" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
  --exec "OPEN ${fx}/ambient.mod")
pc_line(lh "${oh}")
if(NOT lh MATCHES "^amiga amiga ")
  message(FATAL_ERROR "playing mod: want np=amiga bar=amiga, got: ${lh}\n${oh}")
endif()
dui_run(op --db "${WORK}/p.db" --ui desktop --backend mem --view nowplaying --canvas 1280x800 --clock fixed --stats
  --exec "OPEN ${fx}/ambient.mod")
pc_line(lp "${op}")
if(NOT lp MATCHES "^[a-z0-9_]+ [a-z0-9_]+ amiga ")
  message(FATAL_ERROR "playing mod: want page=amiga, got: ${lp}\n${op}")
endif()

# 4: real art wins.
set(music "${HERE}/fixtures/art-music")
set(a "${music}/Aurora Fields/Glass Rivers/01 - Glass Rivers Track 1.mp3")
set(common --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${WORK}/library.db")
file(WRITE "${WORK}/seed.txt" "wait 1\nexec LIBRARY_SCAN ${music}\nwait 8\n")
dui_run(seed ${common} --input "${WORK}/seed.txt")
file(WRITE "${WORK}/play.txt" "exec QUEUE_ADD \"${a}\"\nexec PLAY\nwait 5\n")
dui_run(oa ${common} --input "${WORK}/play.txt")
pc_line(la "${oa}")
if(NOT la MATCHES "^none none ")
  message(FATAL_ERROR "covered song: want np=none bar=none, got: ${la}\n${oa}")
endif()

# 5 (I691b2): the word follows design section 2: only on covers of 64 logical px and up, sized edge/7 snapped to the type scale.
#   The `platform_cover_words` line holds the logical font px of the word each site drew (0 = no word). At scale 2 the player bar
#   thumb is 112 canvas px but 56 logical: no word. The Now Playing panel is large: a word of at least 14 px.
function(word_line outvar out)
  if(NOT "${out}" MATCHES "platform_cover_words np=([0-9]+) bar=([0-9]+) page=([0-9]+) tile0=([0-9]+)\n")
    message(FATAL_ERROR "no platform_cover_words line in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4}" PARENT_SCOPE)
endfunction()
foreach(sc 1 2)
  math(EXPR cw "1280 * ${sc}")
  math(EXPR ch "800 * ${sc}")
  dui_run(ow --db "${WORK}/w${sc}.db" --ui desktop --backend mem --view home --canvas ${cw}x${ch} --scale ${sc} --clock fixed --stats
    --exec "OPEN ${fx}/ambient.mod")
  pc_line(lw "${ow}")
  if(NOT lw MATCHES "^amiga amiga ")
    message(FATAL_ERROR "scale ${sc}: want np=amiga bar=amiga, got: ${lw}\n${ow}")
  endif()
  word_line(ww "${ow}")
  separate_arguments(ww)
  list(GET ww 0 w_np)
  list(GET ww 1 w_bar)
  if(NOT w_bar EQUAL 0)
    message(FATAL_ERROR "scale ${sc}: the player bar thumb drew a word (${w_bar} px); it is under 64 logical px\n${ow}")
  endif()
  if(w_np LESS 14)
    message(FATAL_ERROR "scale ${sc}: the Now Playing cover word is ${w_np} px; a large cover wants edge/7 (at least 14)\n${ow}")
  endif()
endforeach()
