# dui-kind-pages (I678f): album, artist and composer open as the collection page with their kind (design doc 2026-10-29 section 8),
# through the real desktop UI entry. -DSELECTA -DSONGS -DWORK
# Fixture fixture_library.sql plus: songs 7 to 11 (Chris Huelsbeck, Lotus Turbo Challenge), song 12 (Chris Huelsbeck on album 3, so the
# artist "Appears On" album 3), album 5 Second Quest by Chris Huelsbeck with song 13, composer rows for songs 1 and 5 under one canon
# written two ways. So: album 1 has 7 songs and one "More by" album; the artist has 9 songs, 2 albums and 1 appearance; the composer
# has 2 songs and 2 spellings.
# The dump prints `collection kind=<kind> opens= ... of=` and, for a kind page, `kindpage label= key=[] sub=[] queries= popular= rows=
# shelf albums= appears= more= spellings= showmore=x,y,w,h albumcard=x,y,w,h morecard=x,y,w,h`.
# 1. Album: a click on the sidebar row opens page=collection kind=album, the songs equal the sqlite3 count, the "More by" shelf holds 1
#    album and the Albums shelf none; the engine counted the query (queries >= 1, the sentinel that the new code ran).
# 2. Artist: kind=artist, the songs equal the sqlite3 count, Popular shows 5 rows and Show more raises it to 9; Albums holds 2 and
#    Appears On 1.
# 3. A click on the first card of the Albums shelf opens that album's page (kind=album).
# 4. Composer: kind=composer, 2 songs, 2 spellings, no Albums shelf; the Library page's composer row opens the same page and the
#    old drilldown (the "drill" and "back" lines, "Back to all composers") is gone.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-kind-pages needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x1400 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
string(CONCAT seed
  "UPDATE songs SET key = 'local:' || path WHERE key LIKE 'fixture:%'; "
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, added) VALUES"
  "('local:fixture/7.mod','local','fixture/7.mod','Seven','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1),"
  "('local:fixture/8.mod','local','fixture/8.mod','Eight','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1),"
  "('local:fixture/9.mod','local','fixture/9.mod','Nine','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1),"
  "('local:fixture/10.mod','local','fixture/10.mod','Ten','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1),"
  "('local:fixture/11.mod','local','fixture/11.mod','Eleven','Chris Huelsbeck','Lotus Turbo Challenge','mod',1992,1),"
  "('local:fixture/12.mod','local','fixture/12.mod','Twelve','Chris Huelsbeck','Strange Sounds from Beyond','mod',1991,1),"
  "('local:fixture/13.mod','local','fixture/13.mod','Thirteen','Chris Huelsbeck','Second Quest','mod',1995,1); "
  "INSERT INTO albums(id, name, artist, year, source) VALUES(5, 'Second Quest', 'Chris Huelsbeck', 1995, 'local'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('local:fixture/1.mod', 'chris huelsbeck', 'Chris Huelsbeck'); "
  "INSERT INTO song_composers(song_key, canon, display) VALUES('local:fixture/5.sid', 'chris huelsbeck', 'Huelsbeck, Chris'); "
  "INSERT INTO history(key, played_at) VALUES('local:fixture/5.sid', 100),('local:fixture/5.sid', 200),('local:fixture/1.mod', 300)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the kind pages failed: ${err}")
endif()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()
function(sql_count out query)
  execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${query}" RESULT_VARIABLE rc OUTPUT_VARIABLE res ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "count failed: ${err}")
  endif()
  string(STRIP "${res}" res)
  set(${out} "${res}" PARENT_SCOPE)
endfunction()
# The centre of the sidebar row of kind `kind` called `name` into ${out}.
function(side_row out dump kind name)
  if(NOT "${dump}" MATCHES "sidelib_row [0-9]+ ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) pinned=[01] open=[01] kind=${kind} ${name}\n")
    message(FATAL_ERROR "no sidebar row of kind ${kind} '${name}' in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()
# One field of the kindpage line.
function(kp_field out dump field)
  if(NOT "${dump}" MATCHES "kindpage ([^\n]* )?${field}=([^ \n]*)")
    message(FATAL_ERROR "no kindpage line with ${field} in:\n${dump}")
  endif()
  set(${out} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()
function(kp_rect out dump name)
  if(NOT "${dump}" MATCHES "kindpage [^\n]* ${name}=(-?[0-9]+),(-?[0-9]+),([0-9]+),([0-9]+)")
    message(FATAL_ERROR "no ${name} rectangle in:\n${dump}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${out} "${cx} ${cy}" PARENT_SCOPE)
endfunction()
function(col_field out dump field)
  if(NOT "${dump}" MATCHES "collection kind=[a-z]+ [^\n]*${field}=([^ \n]*)")
    message(FATAL_ERROR "no collection line with ${field} in:\n${dump}")
  endif()
  set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()
# Checks the page is the collection page of `kind`.
function(expect_kind dump kind label)
  if(NOT "${dump}" MATCHES "page=collection")
    message(FATAL_ERROR "the page is not the collection page:\n${dump}")
  endif()
  if(NOT "${dump}" MATCHES "collection kind=${kind} ")
    message(FATAL_ERROR "the collection page is not kind ${kind}:\n${dump}")
  endif()
  kp_field(lbl "${dump}" label)
  expect_eq("label of the ${kind} page" "${lbl}" "${label}")
  kp_field(q "${dump}" queries)
  if(q LESS 1)
    message(FATAL_ERROR "the ${kind} page never reached the engine collection query (queries=${q})")
  endif()
  col_field(op "${dump}" opens)
  # Every open path (sidebar row, Library row, card, tile) sends COLLECTION_OPEN. Each run is its own process, so opens >= 1 is the
  # assertion, not an exact count.
  if(op LESS 1)
    message(FATAL_ERROR "the ${kind} page never counted a COLLECTION_OPEN (opens=${op})")
  endif()
endfunction()

set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x1400)
file(WRITE "${WORK}/idle.txt" "wait 100\n")
# Each open touches Recents and reorders the sidebar, so the row is read from an idle run right before the run that clicks it.
function(open_run out tag kind name script)
  dui_run(idle_${tag} ${common} --input "${WORK}/idle.txt")
  side_row(xy "${idle_${tag}}" ${kind} "${name}")
  file(WRITE "${WORK}/${tag}.txt" "click ${xy}\nwait 100\n${script}")
  dui_run(run_${tag} ${common} --input "${WORK}/${tag}.txt")
  set(${out} "${run_${tag}}" PARENT_SCOPE)
endfunction()

# 1. Album.
open_run(alb album 4 "Lotus Turbo Challenge" "")
expect_kind("${alb}" album Album)
sql_count(want_alb "SELECT COUNT(*) FROM songs s JOIN albums a ON a.id=1 AND s.album IS a.name AND s.artist IS a.artist")
col_field(of_alb "${alb}" of)
expect_eq("album songs" "${of_alb}" "${want_alb}")
kp_field(alb_more "${alb}" more)
expect_eq("More by shelf" "${alb_more}" "1")
kp_field(alb_albums "${alb}" albums)
expect_eq("an album has no Albums shelf" "${alb_albums}" "0")
kp_field(alb_year "${alb}" year)
expect_eq("album year in the header" "${alb_year}" "1992")
kp_rect(alb_shuffle "${alb}" shuffle)
# Play starts the first song and queues the rest: all 7 songs are reached, so the queue holds 6 (not a capped slice).
math(EXPR want_queued "${want_alb} - 1")
if(NOT "${alb}" MATCHES "plpage [^\n]* play=(-?[0-9]+),(-?[0-9]+),([0-9]+),([0-9]+)")
  message(FATAL_ERROR "no Play rectangle on the album page:\n${alb}")
endif()
math(EXPR play_x "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR play_y "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
open_run(albplay album 4 "Lotus Turbo Challenge" "click ${play_x} ${play_y}\nwait 100\n")
if(NOT "${albplay}" MATCHES "[ ]queue=([0-9]+) status=")
  message(FATAL_ERROR "no queue count in:\n${albplay}")
endif()
expect_eq("Play queues the rest of the album" "${CMAKE_MATCH_1}" "${want_queued}")
open_run(albshuf album 4 "Lotus Turbo Challenge" "click ${alb_shuffle}\nwait 100\n")
if(NOT "${albshuf}" MATCHES "[ ]queue=([0-9]+) status=")
  message(FATAL_ERROR "no queue count in:\n${albshuf}")
endif()
expect_eq("Shuffle queues the rest of the album" "${CMAKE_MATCH_1}" "${want_queued}")

# 2. Artist.
open_run(art artist 5 "Chris Huelsbeck" "")
expect_kind("${art}" artist Artist)
sql_count(want_art "SELECT COUNT(*) FROM songs WHERE artist='Chris Huelsbeck'")
col_field(of_art "${art}" of)
expect_eq("artist songs" "${of_art}" "${want_art}")
kp_field(pop "${art}" popular)
expect_eq("Popular limit" "${pop}" "5")
kp_field(rows "${art}" rows)
expect_eq("Popular rows" "${rows}" "5")
kp_field(art_albums "${art}" albums)
expect_eq("Albums shelf" "${art_albums}" "2")
kp_field(art_app "${art}" appears)
expect_eq("Appears On shelf" "${art_app}" "1")
kp_rect(more_xy "${art}" showmore)
open_run(art2 artist2 5 "Chris Huelsbeck" "click ${more_xy}\nwait 100\n")
kp_field(pop2 "${art2}" popular)
expect_eq("Popular limit after Show more" "${pop2}" "10")
kp_field(rows2 "${art2}" rows)
expect_eq("Popular rows after Show more" "${rows2}" "${want_art}")

# 3. The first Albums card opens that album's page.
kp_rect(card_xy "${art}" albumcard)
open_run(card card 5 "Chris Huelsbeck" "click ${card_xy}\nwait 100\n")
expect_kind("${card}" album Album)

# 4. Composer: the sidebar row, then the Library page's row; no drilldown anywhere.
open_run(com comp 6 "Chris Huelsbeck" "")
expect_kind("${com}" composer Composer)
sql_count(want_com "SELECT COUNT(*) FROM song_composers WHERE canon='chris huelsbeck'")
col_field(of_com "${com}" of)
expect_eq("composer songs" "${of_com}" "${want_com}")
kp_field(com_sp "${com}" spellings)
expect_eq("Spellings" "${com_sp}" "2")
# The header shows the display name (first spelling), not the canon key "chris huelsbeck".
if(NOT "${com}" MATCHES "kindpage [^\n]* name=\\[([^]]*)\\] year")
  message(FATAL_ERROR "no name field in the composer kindpage line:\n${com}")
endif()
expect_eq("composer display name" "${CMAKE_MATCH_1}" "Chris Huelsbeck")
kp_field(com_alb "${com}" albums)
expect_eq("a composer has no Albums shelf" "${com_alb}" "0")
dui_run(lib_list --start-dir "${SONGS}" --db "${db}" --ui desktop --backend mem --view library --canvas 900x720 --clock fixed --stats --input "${WORK}/idle.txt")
if(NOT "${lib_list}" MATCHES "composer ([0-9]+) chris huelsbeck\\|")
  message(FATAL_ERROR "no composer row for chris huelsbeck in:\n${lib_list}")
endif()
set(cidx "${CMAKE_MATCH_1}")
if(NOT "${lib_list}" MATCHES "row ${cidx} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no row rectangle for composer ${cidx} in:\n${lib_list}")
endif()
math(EXPR rx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR ry "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
file(WRITE "${WORK}/libclick.txt" "click ${rx} ${ry}\nwait 100\n")
dui_run(lib_click --start-dir "${SONGS}" --db "${db}" --ui desktop --backend mem --view library --canvas 900x720 --clock fixed --stats --input "${WORK}/libclick.txt")
expect_kind("${lib_click}" composer Composer)
foreach(gone "Back to all composers" "\ndrill " "\nback ")
  string(FIND "${lib_click}" "${gone}" at)
  if(NOT at LESS 0)
    message(FATAL_ERROR "the composer drilldown is still there ('${gone}'):\n${lib_click}")
  endif()
endforeach()

# 5. I678g: the pinned rows and a source. Liked Songs, Recently Played and Local Files show the wordmark header, a source page the
# status block (design doc section 8). Seed: two liked songs (12 newer than 7), two songs of an unregistered source "hvsc".
string(CONCAT seed2 "INSERT INTO liked(key, at) VALUES('local:fixture/7.mod', 10),('local:fixture/12.mod', 50); "
  "INSERT INTO songs(key, source, path, name, artist, album, format, year, added) VALUES"
  "('hvsc:a.sid','hvsc','a.sid','HvscA','Rob Hubbard','','sid',1986,1),('hvsc:b.sid','hvsc','b.sid','HvscB','Rob Hubbard','','sid',1986,1)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed2}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the pinned and source pages failed: ${err}")
endif()
# One field of the kindhead line.
function(kh_field out dump field)
  if(NOT "${dump}" MATCHES "kindhead ([^\n]* )?${field}=(\\[[^]]*\\]|[^ \n]*)")
    message(FATAL_ERROR "no kindhead line with ${field} in:\n${dump}")
  endif()
  string(REGEX REPLACE "^\\[(.*)\\]$" "\\1" v "${CMAKE_MATCH_2}")
  set(${out} "${v}" PARENT_SCOPE)
endfunction()
# kind row-name page-kind label wordmark played source count-query first-key
function(expect_pinned tag kind rowname pkind label wordmark played source countq first)
  open_run(pg ${tag} ${kind} "${rowname}" "")
  expect_kind("${pg}" ${pkind} ${label})
  sql_count(want "${countq}")
  col_field(got "${pg}" of)
  expect_eq("${tag} songs" "${got}" "${want}")
  kh_field(w "${pg}" wordmark)
  expect_eq("${tag} wordmark header" "${w}" "${wordmark}")
  kh_field(p "${pg}" played)
  expect_eq("${tag} Played column" "${p}" "${played}")
  kh_field(s "${pg}" source)
  expect_eq("${tag} source block" "${s}" "${source}")
  kh_field(f "${pg}" first)
  expect_eq("${tag} first song" "${f}" "${first}")
  set(pg "${pg}" PARENT_SCOPE)
endfunction()
expect_pinned(liked 0 "Liked Songs" liked Playlist 1 0 0 "SELECT COUNT(*) FROM liked" "local:fixture/12.mod")
expect_pinned(recent 1 "Recently Played" recent Playlist 1 1 0 "SELECT COUNT(DISTINCT h.key) FROM history h JOIN songs s ON s.key=h.key" "local:fixture/1.mod")
expect_pinned(local 2 "Local Files" local Source 1 0 0 "SELECT COUNT(*) FROM songs WHERE source='local'" "local:fixture/1.mod")
expect_pinned(src 7 "HVSC" source Source 0 0 1 "SELECT COUNT(*) FROM songs WHERE source='hvsc'" "hvsc:a.sid")
kh_field(status "${pg}" status)
expect_eq("the registered remote source status" "${status}" "Remote, never synced")
