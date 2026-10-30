# dui-platform-covers (I691b): a song without real art shows the platform cover of its file name's family. -DSELECTA -DWORK
#   Design: thoughts/shared/research/2026-10-29-i691-platform-covers.md sections 3 and 4. Violated invariant: the song sites drew the
#   anonymous genart gradient. The dump line `platform_covers np= bar= page= tile0= draws= shelf= grid= flow= side= header=` names the family (lower case id) each site
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
  if(NOT "${out}" MATCHES "platform_covers np=([a-z0-9_]+) bar=([a-z0-9_]+) page=([a-z0-9_]+) tile0=([a-z0-9_]+) draws=([0-9]+) shelf=([a-z0-9_]+) grid=([a-z0-9_]+) flow=([a-z0-9_]+) side=([a-z0-9_]+) header=[a-z0-9_]+\n")
    message(FATAL_ERROR "no platform_covers line in:\n${out}")
  endif()
  set(${outvar} "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${CMAKE_MATCH_3} ${CMAKE_MATCH_4} ${CMAKE_MATCH_5}" PARENT_SCOPE)
  set(more "shelf=${CMAKE_MATCH_6} grid=${CMAKE_MATCH_7} flow=${CMAKE_MATCH_8} side=${CMAKE_MATCH_9}")
  string(REGEX MATCH "header=([a-z0-9_]+)\n" ignored "${out}") # CMake keeps only nine match groups
  set(${outvar}_more "${more} header=${CMAKE_MATCH_1}" PARENT_SCOPE)
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
# I693b (Johan 2026-10-29): the Now Playing fade covers the bottom third, where the family word sits, so a platform cover is not faded.
if(NOT "${oh}" MATCHES "nowplaying fade 0\n")
  message(FATAL_ERROR "playing mod: want nowplaying fade 0 over the platform cover, got:\n${oh}")
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
if(NOT "${oa}" MATCHES "nowplaying fade [1-9][0-9]*\n")
  message(FATAL_ERROR "covered song: want the Now Playing fade over real art, got:\n${oa}")
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

# 6 (I691d): the album and person sites. Fixture library (fixture_library.sql: albums 1 to 3 carry real art, album 4 none) plus an uncovered
#   Amiga album "Uncovered" (two mod songs, no artwork). Each site draws the platform cover of the row's family, and a covered album wins
#   with its art (`none`): Home shelf album tile, Library grid tile, cover flow, album page header, each a separate run because the dump
#   names the last tile a site drew.
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-platform-covers needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/mixed.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%'; "
  "INSERT INTO albums(id, name, artist, year, source) VALUES(5, 'Uncovered', 'Nobody', 1990, 'local'); "
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, added) VALUES"
  "('local:fixture/7.mod','local','fixture/7.mod','Seven','Nobody','Uncovered','mod',1990,1),"
  "('local:fixture/8.mod','local','fixture/8.mod','Eight','Nobody','Uncovered','mod',1990,1)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the mixed library failed: ${err}")
endif()

# field(out dump key): one key=value of the platform_covers line.
function(pc_field out dump key)
  pc_line(l "${dump}")
  if(NOT "${l_more}" MATCHES "${key}=([a-z0-9_]+)")
    message(FATAL_ERROR "no ${key} in ${l_more}")
  endif()
  set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
function(pc_draws out dump)
  pc_line(l "${dump}")
  separate_arguments(l)
  list(GET l 4 n)
  set(${out} "${n}" PARENT_SCOPE)
endfunction()
function(expect_site dump key want what)
  pc_field(got "${dump}" ${key})
  pc_draws(n "${dump}")
  if(NOT got STREQUAL "${want}")
    message(FATAL_ERROR "${what}: ${key}=${got}, want ${want}\n${dump}")
  endif()
  if(NOT want STREQUAL "none" AND n LESS 1)
    message(FATAL_ERROR "${what}: draws=${n}: the platform cover never ran\n${dump}")
  endif()
endfunction()

# Home shelf: a skipped song puts its album on the Jump back in shelf; the uncovered mod album is an Amiga tile.
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}"
  "INSERT INTO history(key, played_at, skipped) VALUES('local:fixture/7.mod', 100, 1),('local:fixture/8.mod', 200, 1)")
dui_run(shelf_u --db "${db}" --ui desktop --backend mem --view home --canvas 1280x1400 --clock fixed --stats)
expect_site("${shelf_u}" shelf amiga "Home shelf album tile")

# Library grid and cover flow: queue one song per run, so the last tile drawn is that song's album.
function(grid_run tag style song)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "exec VIEW_SOURCE queue\nexec VIEW_WINDOW 8\nexec VIEW_STYLE ${style}\nexec COMPOSERS_SHOW\nwait 100\nwait 600\n")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD \"fixture/${song}\"" --input "${WORK}/${tag}.txt")
  set(${tag} "${o}" PARENT_SCOPE)
endfunction()
grid_run(g_unc grid 7.mod)
expect_site("${g_unc}" grid amiga "Library grid, uncovered mod album")
grid_run(g_cov grid 1.mod)
expect_site("${g_cov}" grid none "Library grid, covered album")
grid_run(f_unc coverflow 7.mod)
expect_site("${f_unc}" flow amiga "cover flow, uncovered mod album")
grid_run(f_cov coverflow 1.mod)
expect_site("${f_cov}" flow none "cover flow, covered album")

# Album page header: the sidebar row of the uncovered album opens its page.
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x1400)
file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(idle ${common} --input "${WORK}/idle.txt")
if(NOT "${idle}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=4 Uncovered\n")
  message(FATAL_ERROR "no sidebar row of the Uncovered album in:\n${idle}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
file(WRITE "${WORK}/open.txt" "click ${cx} ${cy}\nwait 100\n")
dui_run(hdr ${common} --input "${WORK}/open.txt")
expect_site("${hdr}" header amiga "album page header")

# 7 (I691f, Johan 2026-10-29): the cover field is a gradient from a dark shade at the top to the family tone at the bottom (design
#   section 2, Field), and the Now Playing panel shows the header band around the cover (above it and on both sides, tinted from the
#   cover's average colour). Pixels come from the frame dump of the playing-mod home view at 1280x800 (RGBA32 canvas, PPM header 16 bytes).
dui_run(og --db "${WORK}/g.db" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
  --frame-dump "${WORK}/g.ppm" --exec "OPEN ${fx}/ambient.mod")
if(NOT "${og}" MATCHES "nowplaying art ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) panel")
  message(FATAL_ERROR "no nowplaying art line in:\n${og}")
endif()
set(ax ${CMAKE_MATCH_1})
set(ay ${CMAKE_MATCH_2})
set(aw ${CMAKE_MATCH_3})
set(ah ${CMAKE_MATCH_4})
function(gpixel outvar x y)
  math(EXPR off "16 + (${y} * 1280 + ${x}) * 3")
  file(READ "${WORK}/g.ppm" hex OFFSET ${off} LIMIT 3 HEX)
  set(${outvar} "${hex}" PARENT_SCOPE)
endfunction()
math(EXPR mx "${ax} + ${aw} / 2")
math(EXPR bottom "${ay} + ${ah} - 2")
math(EXPR above "${ay} - 3")
math(EXPR left "${ax} - 2")
math(EXPR inx "${ax} + 2")
math(EXPR sidey "${ay} + 6")
gpixel(top_px ${mx} ${ay})
gpixel(bot_px ${inx} ${bottom})
if(top_px STREQUAL bot_px)
  message(FATAL_ERROR "the Now Playing cover is flat: top ${top_px} equals bottom ${bot_px}; want a dark-to-tone gradient")
endif()
gpixel(band_px ${mx} ${above})
if(band_px STREQUAL top_px)
  message(FATAL_ERROR "no margin above the Now Playing cover: the band pixel ${band_px} equals the cover's top pixel")
endif()
gpixel(side_px ${left} ${sidey})
gpixel(side_in ${inx} ${sidey})
if(side_px STREQUAL side_in)
  message(FATAL_ERROR "no margin beside the Now Playing cover: the band pixel ${side_px} equals the cover pixel")
endif()
