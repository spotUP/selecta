# dui-cover-slideshow (I1384b): the big cover of the playing song cycles through every cached still of its album, a "Watch demo" button opens
# the dz_video link. -DSELECTA -DSONGS -DWORK -DSQLITE3
#   Violated invariant: the cover slot showed the 'cover' row only; the dz1..dz7 Demozoo screenshots never reached the screen.
#   Scratch library db (WORK/library.db, never ~/.config/selecta): a seed scan of the art-music fixture creates three albums with covers; the
#   artwork table of the song's album then gets two more stills (copies of the other albums' covers, so the three differ) and a dz_video link.
#   The Now Playing page (desktop UI entry point, --view nowplaying) plays the song: the dump line `cover_slide of= frame= switches= hash=`
#   must show 3 stills, frame 0 at the start, a different hash after one art.slideshow_seconds, a third after two, back to the first after three,
#   and the pref (2 s here) must set the interval. A click on the Watch demo button must record the link (SELECTA_NO_OPEN keeps the browser shut).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
dui_require_sdl()
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{SELECTA_NO_OPEN} 1)
set(music "${HERE}/fixtures/art-music")
set(a "${music}/Aurora Fields/Glass Rivers/01 - Glass Rivers Track 1.mp3")
set(db "${WORK}/library.db")
set(common --ui desktop --backend mem --view nowplaying --canvas 1280x800 --clock fixed --stats --db "${db}")
file(WRITE "${WORK}/seed.txt" "wait 1\nexec LIBRARY_SCAN ${music}\nwait 8\n")
dui_run(seed ${common} --input "${WORK}/seed.txt")
file(WRITE "${WORK}/art.sql" "
INSERT OR REPLACE INTO artwork(album_id, kind, data)
  SELECT (SELECT album_id FROM artwork WHERE kind = 'cover' ORDER BY album_id LIMIT 1), 'dz1', data
  FROM artwork WHERE kind = 'cover' ORDER BY album_id LIMIT 1 OFFSET 1;
INSERT OR REPLACE INTO artwork(album_id, kind, data)
  SELECT (SELECT album_id FROM artwork WHERE kind = 'cover' ORDER BY album_id LIMIT 1), 'dz2', data
  FROM artwork WHERE kind = 'cover' ORDER BY album_id LIMIT 1 OFFSET 2;
INSERT OR REPLACE INTO artwork(album_id, kind, data)
  VALUES((SELECT album_id FROM artwork WHERE kind = 'cover' ORDER BY album_id LIMIT 1), 'dz_video', CAST('https://example.invalid/demo' AS BLOB));
")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${WORK}/art.sql" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "fixture sql failed: ${err}")
endif()
# the song of the album that now holds the stills: the first album by id is the one the sql used; find which fixture song it is
execute_process(COMMAND "${SQLITE3}" "${db}" "SELECT s.path FROM songs s JOIN albums a ON a.name = s.album AND a.artist IS NULLIF(s.artist, '') WHERE a.id = (SELECT album_id FROM artwork WHERE kind = 'cover' ORDER BY album_id LIMIT 1) ORDER BY s.path LIMIT 1"
  OUTPUT_VARIABLE song RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT rc EQUAL 0 OR "${song}" STREQUAL "")
  message(FATAL_ERROR "no song for the stills album (rc=${rc}): ${err}")
endif()
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\nart.slideshow_seconds=2\n")

function(run_at tag waits)
  file(WRITE "${WORK}/${tag}.txt" "exec QUEUE_ADD \"${song}\"\nexec REPEAT one\nexec PLAY\nwait 600\n${waits}")
  dui_run(o ${common} --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "cover_slide of=([0-9]+) frame=([0-9]+) switches=([0-9]+) hash=([0-9a-f]+) watch=(-?[0-9]+),(-?[0-9]+),(-?[0-9]+) clicks=([0-9]+) last=([^\n]*)\n")
    message(FATAL_ERROR "${tag}: no cover_slide line in:\n${o}")
  endif()
  set(${tag}_of ${CMAKE_MATCH_1} PARENT_SCOPE)
  set(${tag}_frame ${CMAKE_MATCH_2} PARENT_SCOPE)
  set(${tag}_sw ${CMAKE_MATCH_3} PARENT_SCOPE)
  set(${tag}_hash ${CMAKE_MATCH_4} PARENT_SCOPE)
  set(${tag}_wx ${CMAKE_MATCH_5} PARENT_SCOPE)
  set(${tag}_wy ${CMAKE_MATCH_6} PARENT_SCOPE)
  set(${tag}_watch ${CMAKE_MATCH_7} PARENT_SCOPE)
  set(${tag}_clicks ${CMAKE_MATCH_8} PARENT_SCOPE)
  set(${tag}_last "${CMAKE_MATCH_9}" PARENT_SCOPE)
  # I1455: the cover spinner dump line exists and agrees with the show
  if(NOT "${o}" MATCHES "cover_spinner drawn=([0-9]+) slide_switches=([0-9]+) slide_of=([0-9]+)")
    message(FATAL_ERROR "${tag}: no cover_spinner line in:\n${o}")
  endif()
  set(${tag}_spof ${CMAKE_MATCH_3} PARENT_SCOPE)
endfunction()

run_at(t0 "wait 400\nwait 100\n")
run_at(t1 "wait 2400\nwait 100\n")
run_at(t2 "wait 4400\nwait 100\n")
run_at(t3 "wait 6400\nwait 100\n")
message(STATUS "frames of=${t0_of} f0=${t0_frame}/${t0_hash} f1=${t1_frame}/${t1_hash} f2=${t2_frame}/${t2_hash} f3=${t3_frame}/${t3_hash}")
if(NOT t0_of EQUAL 3)
  message(FATAL_ERROR "the album holds 3 stills, the slide show counted ${t0_of}")
endif()
if(NOT t0_frame EQUAL 0 OR NOT t1_frame EQUAL 1 OR NOT t2_frame EQUAL 2 OR NOT t3_frame EQUAL 0)
  message(FATAL_ERROR "frame order ${t0_frame} ${t1_frame} ${t2_frame} ${t3_frame}, want 0 1 2 0 at 2 s steps")
endif()
if(t0_hash STREQUAL t1_hash OR t1_hash STREQUAL t2_hash OR t0_hash STREQUAL t2_hash)
  message(FATAL_ERROR "three stills must draw three distinct frames: ${t0_hash} ${t1_hash} ${t2_hash}")
endif()
if(NOT t3_hash STREQUAL t0_hash)
  message(FATAL_ERROR "the show must wrap to the first still: ${t3_hash} vs ${t0_hash}")
endif()
if(NOT t0_spof EQUAL 3)
  message(FATAL_ERROR "cover_spinner dump slide_of=${t0_spof}, want 3")
endif()
# Watch demo: the button exists and a click records the link
if(t0_watch LESS 1)
  message(FATAL_ERROR "no Watch demo button although the album holds a dz_video link")
endif()
run_at(t4 "wait 100\nclick ${t0_wx} ${t0_wy}\nwait 100\n")
if(NOT t4_clicks EQUAL 1 OR NOT t4_last STREQUAL "https://example.invalid/demo")
  message(FATAL_ERROR "Watch demo click: clicks=${t4_clicks} last=${t4_last}")
endif()
message(STATUS "dui-cover-slideshow ok")
