# dui-frame (DUI-P4-1): the desktop frame (sidebar, content, Now Playing panel, transport bar), through the real shell.
#   -DSELECTA -DSONGS -DWORK -DGOLDEN
# 1. `--ui desktop` opens through the plugin registry (kind 5): the ui_stats line carries id=desktop, and frame_parts
#    (raised only by the frame drawing code) is above 0 -- the sentinel that the plugin drew the frame.
# 2. Scripted --input clicks on the transport buttons, found from the layout output (`button id=` lines at 1280x800),
#    change the engine state, read back from the stats: play -> play_state 1 (playing), play + pause -> 2 (paused),
#    play + next -> a different song, shuffle -> random_on 1, repeat once -> repeat_mode 1 (all), twice -> 2 (one).
#    transport_hits counts the clicks that reached the engine.
# 3. One render hash golden at 1280x800 (update only with SELECTA_GOLDEN_UPDATE=1).
# 4. The frame layout id at narrow widths: 800 gives icon-sidebar, 600 gives mini-suggestion.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n banana apple cherry)
  configure_file("${SONGS}/selecta-test.mod" "${WORK}/${n}.mod" COPYONLY)
endforeach()

set(queue --exec "QUEUE_ADD ${WORK}/banana.mod" --exec "QUEUE_ADD ${WORK}/apple.mod" --exec "QUEUE_ADD ${WORK}/cherry.mod")

function(frame_run outvar input)
  # ARGN: more --exec arguments run before the UI opens.
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view songs --canvas 1280x800 --clock fixed --stats ${queue} ${ARGN} --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

function(expect out key want)
  dui_stat(got "${out}" desktop ${key})
  if(NOT got EQUAL want)
    message(FATAL_ERROR "${key}=${got}, expected ${want} in:\n${out}")
  endif()
endfunction()

# Button centres from the layout output.
dui_run(lay --ui desktop --backend mem --canvas 1280x800 --resize 1280x800)
string(REPLACE "\n" ";" laylines "${lay}")
foreach(line IN LISTS laylines)
  if(line MATCHES "^button id=([a-z_]+) x=(-?[0-9]+) y=(-?[0-9]+) w=([0-9]+) h=([0-9]+)$")
    math(EXPR cx_${CMAKE_MATCH_1} "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
    math(EXPR cy_${CMAKE_MATCH_1} "${CMAKE_MATCH_3} + ${CMAKE_MATCH_5} / 2")
  endif()
endforeach()
foreach(id play prev next random_on repeat_all)
  if(NOT DEFINED cx_${id})
    message(FATAL_ERROR "the layout output lists no ${id} button:\n${lay}")
  endif()
endforeach()
set(play "click ${cx_play} ${cy_play}\n")

# 1 and 2. The sentinel and the clicks.
frame_run(idle "")
expect("${idle}" transport_hits 0)
expect("${idle}" play_state 0)
dui_stat(parts "${idle}" desktop frame_parts)
if(parts EQUAL 0)
  message(FATAL_ERROR "frame_parts=0: the frame drawing code did not run:\n${idle}")
endif()

frame_run(played "${play}")
expect("${played}" transport_hits 1)
expect("${played}" play_state 1)

# The engine takes a click's line on the shell's next pump, so a script cannot see its own click land: each run
# starts from the state the click acts on, set by --exec.
frame_run(paused "${play}" --exec "PLAY")
expect("${paused}" transport_hits 1)
expect("${paused}" play_state 2)

frame_run(first "" --exec "PLAY")
frame_run(nexted "click ${cx_next} ${cy_next}\n" --exec "PLAY")
expect("${nexted}" transport_hits 1)
if(NOT first MATCHES "song=([^\n]*)\n")
  message(FATAL_ERROR "no song line after play:\n${first}")
endif()
set(song_first "${CMAKE_MATCH_1}")
if(NOT nexted MATCHES "song=([^\n]*)\n")
  message(FATAL_ERROR "no song line after next:\n${nexted}")
endif()
if(CMAKE_MATCH_1 STREQUAL song_first)
  message(FATAL_ERROR "next did not change the song, still '${song_first}'")
endif()

frame_run(shuffled "click ${cx_random_on} ${cy_random_on}\n")
expect("${shuffled}" transport_hits 1)
expect("${shuffled}" random_on 1)

frame_run(rep1 "click ${cx_repeat_all} ${cy_repeat_all}\n")
expect("${rep1}" repeat_mode 1)
frame_run(rep2 "click ${cx_repeat_all} ${cy_repeat_all}\n" --exec "REPEAT all")
expect("${rep2}" repeat_mode 2)
expect("${rep2}" transport_hits 1)

# 3. The golden render at 1280x800.
frame_run(shot "")
dui_stat(hash "${shot}" desktop canvas_hash)
dui_stat(missing "${shot}" desktop missing_glyphs)
if(NOT missing EQUAL 0)
  message(FATAL_ERROR "missing_glyphs=${missing} in the frame")
endif()
set(gfile "${GOLDEN}/desktop/frame-rgba32-1280x800@1.hash")
if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
  file(WRITE "${gfile}" "${hash}\n")
endif()
if(NOT EXISTS "${gfile}")
  message(FATAL_ERROR "missing golden ${gfile} (hash ${hash}); create it with SELECTA_GOLDEN_UPDATE=1")
endif()
file(STRINGS "${gfile}" want LIMIT_COUNT 1)
if(NOT hash STREQUAL want)
  message(FATAL_ERROR "frame hash ${hash} differs from golden ${want} (${gfile})")
endif()

# 4. Narrow widths pick the compact frame layouts.
dui_run(narrow --ui desktop --backend mem --resize 800x600,600x600)
foreach(spec "800:icon-sidebar" "600:mini-suggestion")
  string(REPLACE ":" ";" pair "${spec}")
  list(GET pair 0 w)
  list(GET pair 1 id)
  if(NOT narrow MATCHES "layout width=${w} height=600 id=${id} ")
    message(FATAL_ERROR "width ${w} must give ${id}:\n${narrow}")
  endif()
endforeach()
message(STATUS "dui-frame: transport clicks, golden ${hash} and narrow layouts pass")

# 5. The Home view chrome (I230, Main.dc.html): each drawn part raises one bit of home_parts, so a missing part fails
#    here. Bits: 1 nav, 2 brand, 4 tile card, 8 play affordance (hover only), 16 player art, 32 player title,
#    64 seek bar with times, 128 greeting, 256 section heading, 512 tile artist line, 1024 header search icon,
#    2048 header tint, 4096 panels, 8192 chips. tile_cards counts the card surfaces (one per queued song).
function(home_run outvar input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats ${queue} --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()
home_run(homeidle "")
dui_stat(hp "${homeidle}" desktop home_parts)
math(EXPR rest "${hp} & 16375")
if(NOT rest EQUAL 16375 OR NOT hp EQUAL 16375)
  message(FATAL_ERROR "home_parts=${hp}, expected 16375 (every chrome part but the hover play affordance):\n${homeidle}")
endif()
# The counter adds up over frames and --view home enters the page in a frame of its own (I266): a whole number of the three queued cards.
dui_stat(tc "${homeidle}" desktop tile_cards)
math(EXPR tc_rest "${tc} % 3")
if(tc EQUAL 0 OR NOT tc_rest EQUAL 0)
  message(FATAL_ERROR "tile_cards=${tc}, expected a positive multiple of the 3 queued cards:\n${homeidle}")
endif()
home_run(homehover "hover 0\nwait 300\n")
dui_stat(hp2 "${homehover}" desktop home_parts)
math(EXPR hover_bit "${hp2} & 8")
if(NOT hover_bit EQUAL 8)
  message(FATAL_ERROR "home_parts=${hp2}: hovering a tile did not draw the play affordance:\n${homehover}")
endif()
