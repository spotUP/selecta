# dui-home-shelves (I678j): the Home feed's item shelves of design doc section 11, through the real desktop UI entry. -DSELECTA -DWORK
# Fixture fixture_home_shelves.sql: three albums with 3 / 1 / 2 plays, one playlist, one liked song.
# 1. The heading order of the item shelves is Recently played items, Your albums, Your playlists (only those with rows).
# 2. Quick access holds 8 tiles (liked, playlist, 3 albums, 3 artists = the cap) and tile 0 is "Liked Songs".
# 3. Your albums lists Alb1, Alb3, Alb2 (most played first).
# 4. A click on the Your albums card Alb1 opens `page=collection`, `collection kind=album`, the page named Alb1.
# 5. Sentinel: home_item_shelves > 0 (the engine sent item shelves).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-home-shelves needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/lib.db")
dui_run(init --ui desktop --backend mem --view home --canvas 1280x1400 --clock fixed --stats --db "${db}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_home_shelves.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

file(WRITE "${WORK}/idle.txt" "wait 100\n")
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x1400)
dui_run(m1 ${common} --input "${WORK}/idle.txt")

# 1. Heading order.
string(REGEX MATCHALL "shelf [0-9]+ tile 0 [^|\n]*" heads "${m1}")
set(order "")
foreach(h IN LISTS heads)
  string(REGEX REPLACE "^shelf [0-9]+ tile 0 " "" name "${h}")
  if(name MATCHES "^(Recently Played|Your Albums|Your Composers|Your Playlists)$")
    list(APPEND order "${name}")
  endif()
endforeach()
expect_eq("item shelf order" "${order}" "Recently Played;Your Albums;Your Composers;Your Playlists")

# 2. Quick access.
string(REGEX MATCHALL "\nquick [0-9]+ cols=[0-9]+ [^\n]*" quick "${m1}")
list(LENGTH quick nq)
expect_eq("quick access tiles" "${nq}" "8")
list(GET quick 0 q0)
if(NOT q0 MATCHES "\\|Liked Songs\\|")
  message(FATAL_ERROR "quick tile 0 is not Liked Songs: ${q0}")
endif()

# 3. Your albums order.
string(REGEX MATCHALL "shelf [0-9]+ tile [0-9]+ Your Albums\\|[^|\n]*\\|[^|\n]*" albums "${m1}")
set(names "")
foreach(a IN LISTS albums)
  string(REGEX REPLACE "^.*\\|([^|]*)$" "\\1" n "${a}")
  list(APPEND names "${n}")
endforeach()
list(GET names 0 1 2 first3)
expect_eq("Your albums order" "${first3}" "Alb1;Alb3;Alb2")

# 4. Click the first Your albums card.
list(GET albums 0 first)
string(REGEX MATCH "shelf ([0-9]+) tile 0 " m "${first}")
set(sidx "${CMAKE_MATCH_1}")
if(NOT m1 MATCHES "shelf_at ${sidx} 0 ([0-9]+) ([0-9]+)")
  message(FATAL_ERROR "no shelf_at line for shelf ${sidx} in:\n${m1}")
endif()
file(WRITE "${WORK}/click.txt" "click ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\nwait 300\n")
dui_run(m2 ${common} --input "${WORK}/click.txt")
if(NOT m2 MATCHES "page=collection")
  message(FATAL_ERROR "card click did not open the collection page:\n${m2}")
endif()
if(NOT m2 MATCHES "collection kind=album ")
  message(FATAL_ERROR "card click did not open an album page:\n${m2}")
endif()
if(NOT m2 MATCHES "plpage id=[0-9]+ name=\\[Alb1\\]")
  message(FATAL_ERROR "album page is not Alb1:\n${m2}")
endif()

# 4b. The chips (section 11) filter the shelves by format family: Music holds the format-less songs, Modules the tracker song,
# Chiptunes the SID song; the chip is found from its label line in the dump and clicked.
function(chip_albums chip want)
  if(NOT m1 MATCHES "label ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ink [0-9]+ [0-9]+ text ${chip}\n")
    message(FATAL_ERROR "no label line for chip ${chip} in:\n${m1}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  file(WRITE "${WORK}/chip.txt" "click ${cx} ${cy}\nwait 300\n")
  dui_run(mc ${common} --input "${WORK}/chip.txt")
  string(REGEX MATCHALL "shelf [0-9]+ tile [0-9]+ Your Albums\\|[^|\n]*\\|[^|\n]*" cal "${mc}")
  set(cn "")
  foreach(a IN LISTS cal)
    string(REGEX REPLACE "^.*\\|([^|]*)$" "\\1" n "${a}")
    list(APPEND cn "${n}")
  endforeach()
  expect_eq("Your Albums under chip ${chip}" "${cn}" "${want}")
endfunction()
chip_albums(Music "Alb1;Alb3;Alb2")
chip_albums(Modules "Alb5")
chip_albums(Chiptunes "Alb4")

# 5. Sentinel.
if(NOT m1 MATCHES "home_item_shelves=([0-9]+)" OR CMAKE_MATCH_1 LESS 1)
  message(FATAL_ERROR "home_item_shelves sentinel missing or 0 in:\n${m1}")
endif()
message(STATUS "dui-home-shelves ok")
