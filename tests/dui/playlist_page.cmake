# dui-playlist-page (I633): a saved playlist opens as a page of its songs, through the real desktop UI entry.
#   -DSELECTA -DSONGS -DWORK
#   Violated invariant: a playlist the user opens (sidebar Your Library row, or a playlist row of the Playlists tree) shows its
#   songs. The sidebar row led to the list of playlists and the tree row did nothing visible.
#   A first run makes a playlist "alpha" from the queue (a.mod) and adds b.mod. The dump prints `plpage id= name=[] songs= rows= draws=` and one
#   `plsong <i> x y w h file= title=` per drawn row.
# 1. A click on the sidebar row opens the page: 2 songs, in playlist order, drawn (draws > 0, rows 2).
# 2. Remove from Playlist (the menu item after the shared five) on row 0 leaves 1 song, b.mod.
# 3. A click on a playlist row of the Playlists tree opens the same page; Escape returns to the tree.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n a b)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/${n}.mod")
endforeach()
set(db "${WORK}/lib.db")

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

function(pp_centre outvar out re)
  if(NOT "${out}" MATCHES "${re} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
    message(FATAL_ERROR "no '${re}' in:\n${out}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  set(${outvar} "${cx} ${cy}" PARENT_SCOPE)
endfunction()

function(pp_page tag out songs)
  if(NOT "${out}" MATCHES "plpage id=([0-9]+) name=\\[alpha\\] songs=([0-9]+) rows=([0-9]+) draws=([0-9]+)")
    message(FATAL_ERROR "no plpage line for alpha in:\n${out}")
  endif()
  expect_eq("${tag} songs" "${CMAKE_MATCH_2}" "${songs}")
  expect_eq("${tag} rows drawn" "${CMAKE_MATCH_3}" "${songs}")
  if(CMAKE_MATCH_4 LESS 1)
    message(FATAL_ERROR "${tag}: the playlist page never drew:\n${out}")
  endif()
endfunction()

dui_run(seed --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}"
  --exec "QUEUE_ADD ${WORK}/a.mod" --exec "PLAYLIST_CREATE alpha"
  --exec "PLAYLIST_ADD_SONG 1 ${WORK}/b.mod")
set(common --ui desktop --backend mem --view home --clock fixed --stats --db "${db}" --canvas 1280x800)

file(WRITE "${WORK}/idle.txt" "wait 100\n")
dui_run(i0 ${common} --input "${WORK}/idle.txt")
pp_centre(side "${i0}" "sidelib_row 0")

# 1. Sidebar row click: the page lists both songs in playlist order.
file(WRITE "${WORK}/open.txt" "click ${side}\nwait 100\n")
dui_run(o1 ${common} --input "${WORK}/open.txt")
pp_page(open "${o1}" 2)
if(NOT "${o1}" MATCHES "plsong 0 [0-9]+ [0-9]+ [0-9]+ [0-9]+ file=a.mod [^\n]*\nplsong 1 [0-9]+ [0-9]+ [0-9]+ [0-9]+ file=b.mod ")
  message(FATAL_ERROR "the page rows are not a.mod then b.mod in:\n${o1}")
endif()

# 2. Right-click row 0, pick the sixth menu item (Remove from Playlist).
pp_centre(row0 "${o1}" "plsong 0")
string(REGEX MATCH "^[0-9]+" rx "${row0}")
string(REGEX MATCH "[0-9]+$" ry "${row0}")
math(EXPR ix "${rx} + 10")
math(EXPR iy "${ry} + 5 * 24 + 12")
file(WRITE "${WORK}/remove.txt" "click ${side}\nwait 50\nmove ${rx} ${ry}\nrclick ${rx} ${ry}\nclick ${ix} ${iy}\nwait 100\n")
dui_run(o2 ${common} --input "${WORK}/remove.txt")
pp_page(removed "${o2}" 1)
if(NOT "${o2}" MATCHES "plsong 0 [0-9]+ [0-9]+ [0-9]+ [0-9]+ file=b.mod ")
  message(FATAL_ERROR "the remaining song is not b.mod in:\n${o2}")
endif()
# The removal reached the library: a later run reads one song.
dui_run(o3 ${common} --input "${WORK}/open.txt")
pp_page(persisted "${o3}" 1)

# 3. The Playlists tree: a playlist row click opens the page, Escape goes back to the tree.
set(tree --ui desktop --backend mem --view playlists --clock fixed --stats --db "${db}" --canvas 1280x800)
dui_run(t0 ${tree} --input "${WORK}/idle.txt")
pp_centre(prow "${t0}" "plslot 0")
file(WRITE "${WORK}/tree.txt" "click ${prow}\nwait 100\n")
dui_run(t1 ${tree} --input "${WORK}/tree.txt")
pp_page(tree "${t1}" 1)
file(WRITE "${WORK}/back.txt" "click ${prow}\nwait 50\nkey escape\nwait 100\n")
dui_run(t2 ${tree} --input "${WORK}/back.txt")
if("${t2}" MATCHES "plpage ")
  message(FATAL_ERROR "Escape did not leave the playlist page:\n${t2}")
endif()
