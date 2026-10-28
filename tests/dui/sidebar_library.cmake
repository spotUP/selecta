# dui-sidebar-library (I608): the "Your Library" list under the sidebar doors, through the real desktop UI entry.
#   -DSELECTA -DSONGS -DWORK
# The dump prints `sidelib draws=<n> rows=<n> items=<n> opens=<n> y=<q16> max=<q16>` and one `sidelib_row <i> x y w h <name>`
# per visible row. A library file with three saved playlists is made by a first run; later runs read it back.
# 1. Wide layout: the list drew (draws > 0) and holds one row per saved playlist.
# 2. Clicking the centre of row 1 counts one open, goes to the Playlists page and selects that playlist.
# 3. Icon-sidebar layout (899 wide): the rows still draw (covers only) and are clickable.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
configure_file("${SONGS}/selecta-test.mod" "${WORK}/one.mod" COPYONLY)
set(db "${WORK}/lib.db")

function(sl_parse tag out)
  if(NOT "${out}" MATCHES "sidelib draws=([0-9]+) rows=([0-9]+) items=([0-9]+) opens=([0-9]+) y=(-?[0-9]+) max=(-?[0-9]+)")
    message(FATAL_ERROR "no sidelib line in:\n${out}")
  endif()
  set(${tag}_draws "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_rows "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_items "${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(${tag}_opens "${CMAKE_MATCH_4}" PARENT_SCOPE)
endfunction()

function(sl_centre outvar out idx)
  if(NOT "${out}" MATCHES "sidelib_row ${idx} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
    message(FATAL_ERROR "no sidelib_row ${idx} in:\n${out}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${outvar} "${cx} ${cy}" PARENT_SCOPE)
endfunction()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

# Seed: three saved playlists.
dui_run(seed --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}"
  --exec "QUEUE_ADD ${WORK}/one.mod" --exec "PLAYLIST_CREATE alpha" --exec "PLAYLIST_CREATE beta" --exec "PLAYLIST_CREATE gamma")

file(WRITE "${WORK}/idle.txt" "wait 100\n")
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}")

# 1. Wide layout: the list drew and holds the three playlists.
dui_run(w1 ${common} --canvas 1280x800 --input "${WORK}/idle.txt")
sl_parse(w1 "${w1}")
if(w1_draws LESS 1)
  message(FATAL_ERROR "the sidebar library list never drew (draws=${w1_draws}) in:\n${w1}")
endif()
expect_eq("library items" "${w1_items}" "3")
expect_eq("library rows" "${w1_rows}" "3")

# 2. A click on row 1 opens that playlist on the Playlists page.
sl_centre(c1 "${w1}" 1)
file(WRITE "${WORK}/click.txt" "click ${c1}\nwait 100\n")
dui_run(w2 ${common} --canvas 1280x800 --input "${WORK}/click.txt")
sl_parse(w2 "${w2}")
expect_eq("opens after click" "${w2_opens}" "1")
if(NOT "${w2}" MATCHES "nav page=playlists")
  message(FATAL_ERROR "the click did not go to the Playlists page:\n${w2}")
endif()

# 3. Icon-sidebar layout: covers only, still clickable.
dui_run(n1 ${common} --canvas 899x700 --input "${WORK}/idle.txt")
sl_parse(n1 "${n1}")
expect_eq("narrow rows" "${n1_rows}" "3")
sl_centre(c2 "${n1}" 1)
file(WRITE "${WORK}/click2.txt" "click ${c2}\nwait 100\n")
dui_run(n2 ${common} --canvas 899x700 --input "${WORK}/click2.txt")
sl_parse(n2 "${n2}")
expect_eq("narrow opens" "${n2_opens}" "1")

# 4. A list longer than the sidebar scrolls with the wheel under the pointer, clamped to its maximum; a short list does not
# take the wheel (the main region keeps it).
set(db2 "${WORK}/long.db")
set(mk --exec "QUEUE_ADD ${WORK}/one.mod")
foreach(i RANGE 1 14)
  list(APPEND mk --exec "PLAYLIST_CREATE list${i}")
endforeach()
dui_run(seed2 --ui desktop --backend mem --view home --canvas 1280x600 --clock fixed --stats --db "${db2}" ${mk})
set(long ${common})
list(REMOVE_ITEM long "${db}")
list(APPEND long "${db2}")
dui_run(l1 ${long} --canvas 1280x600 --input "${WORK}/idle.txt")
sl_parse(l1 "${l1}")
expect_eq("long items" "${l1_items}" "14")
sl_centre(c3 "${l1}" 0)
file(WRITE "${WORK}/wheel.txt" "move ${c3}\nwheel 0 -3\nwait 100\n")
dui_run(l2 ${long} --canvas 1280x600 --input "${WORK}/wheel.txt")
if(NOT "${l2}" MATCHES "sidelib draws=[0-9]+ rows=[0-9]+ items=[0-9]+ opens=[0-9]+ y=([0-9]+) max=([0-9]+)")
  message(FATAL_ERROR "no sidelib line in:\n${l2}")
endif()
if(CMAKE_MATCH_1 LESS 1 OR CMAKE_MATCH_1 GREATER CMAKE_MATCH_2)
  message(FATAL_ERROR "wheel y=${CMAKE_MATCH_1} not within (0, max=${CMAKE_MATCH_2}] in:\n${l2}")
endif()
