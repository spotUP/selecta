# dui-song-menu-multi-playlist (I632 bug 2026-11-07, plan 2026-10-28-multi-select; label select-lists, a case of the multi-select plan).
#   -DSELECTA -DSONGS -DWORK
#   Violated invariant: a song menu opened on a selection acts on the whole selection, and Add to Playlist offers New Playlist...
#   first. The menu added only the right-clicked song (or nothing, with no saved playlist: a dead "No playlists" row).
#   A: select queue rows 0-2 (Shift click), right-click row 1, Add to Playlist, pick the saved playlist `alpha`: the dump shows
#      song_menu_multi n=3 adds=1 list=queue ranges=0-2 and the toast "Added 3 songs to alpha"; a second run opens the playlist page
#      and finds exactly a.mod, b.mod, c.mod in list order (d.mod, the unselected fourth row, is not added).
#   B: the same with New Playlist... (item 0) and no saved playlist: a playlist named New Playlist holds the same three songs.
#   C: a single unselected row, Add to Playlist, New Playlist...: the playlist holds that one song (the single-song path).
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs")
foreach(n a b c d)
  file(COPY_FILE "${SONGS}/selecta-test.mod" "${WORK}/songs/${n}.mod")
endforeach()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

# mp_run(<out> <db> <view> <input> [exec...]): one desktop run on the queue view with the four songs queued.
function(mp_run outvar db view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  set(execs)
  foreach(a ${ARGN})
    list(APPEND execs --exec "${a}")
  endforeach()
  dui_run(o --start-dir "${WORK}/songs" --db "${db}" --ui desktop --backend mem --view ${view} --canvas 1280x800 --clock fixed --stats
          ${execs} --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

set(q_execs "QUEUE_ADD ${WORK}/songs/a.mod" "QUEUE_ADD ${WORK}/songs/b.mod" "QUEUE_ADD ${WORK}/songs/c.mod" "QUEUE_ADD ${WORK}/songs/d.mod")

# Queue row centres (rows 0 and 1).
mp_run(q0 "${WORK}/geo.db" queue "wait 2\n" ${q_execs})
foreach(k 0 1 2)
  if(NOT "${q0}" MATCHES "row ${k} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
    message(FATAL_ERROR "queue: no row ${k} rectangle in:\n${q0}")
  endif()
  math(EXPR rx${k} "${CMAKE_MATCH_1} + 20")
  math(EXPR ry${k} "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
endforeach()

# Menu items sit 24 px apart from the click point: item 3 is Add to Playlist, item 0 of its step is New Playlist..., item 1 the first saved playlist.
math(EXPR ix "${rx1} + 10")
math(EXPR add_y "${ry1} + 3 * 24 + 12")
math(EXPR np_y "${ry1} + 12")
math(EXPR first_y "${ry1} + 24 + 12")
set(select3 "wait 2\nclick ${rx0} ${ry0}\nclick ${rx2} ${ry2} 1 shift\nmove ${rx1} ${ry1}\nrclick ${rx1} ${ry1}\nwait 1\nclick ${ix} ${add_y}\nwait 1\n")

# playlist_songs(<out list var> <db> <expected name>): open the playlist page from the sidebar row and return the file names in page order.
function(playlist_songs outvar db name)
  file(WRITE "${WORK}/idle.txt" "wait 100\n")
  dui_run(i0 --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}" --input "${WORK}/idle.txt")
  if(NOT "${i0}" MATCHES "sidelib_row 3 ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
    message(FATAL_ERROR "no sidelib_row 3 in:\n${i0}")
  endif()
  math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
  math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
  file(WRITE "${WORK}/open.txt" "click ${cx} ${cy}\nwait 100\n")
  dui_run(o1 --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats --db "${db}" --input "${WORK}/open.txt")
  if(NOT "${o1}" MATCHES "plpage id=([0-9]+) name=\\[${name}\\] songs=([0-9]+) ")
    message(FATAL_ERROR "no plpage line for ${name} in:\n${o1}")
  endif()
  set(songs "${CMAKE_MATCH_2}")
  string(REGEX MATCHALL "plsong [0-9]+ [0-9]+ [0-9]+ [0-9]+ [0-9]+ file=[^ \n]*" rows "${o1}")
  set(files)
  foreach(r ${rows})
    string(REGEX REPLACE ".*file=" "" f "${r}")
    list(APPEND files "${f}")
  endforeach()
  set(${outvar} "${songs}:${files}" PARENT_SCOPE)
endfunction()

# A: an existing playlist (made empty: the queue is empty at the time), the whole selection added.
set(dbA "${WORK}/a.db")
mp_run(seedA "${dbA}" home "wait 2\n" "PLAYLIST_CREATE alpha")
mp_run(A "${dbA}" queue "${select3}click ${ix} ${first_y}\nwait 2\n" ${q_execs})
if(NOT "${A}" MATCHES "song_menu_multi n=([0-9]+) adds=([0-9]+) list=([a-z]+) version=([0-9]+) ranges=([^ \n]+)")
  message(FATAL_ERROR "A: no song_menu_multi line in:\n${A}")
endif()
expect_eq("A selection size" "${CMAKE_MATCH_1}" 3)
expect_eq("A SONGS_PLAYLIST_ADD sent once" "${CMAKE_MATCH_2}" 1)
expect_eq("A list" "${CMAKE_MATCH_3}" "queue")
expect_eq("A ranges" "${CMAKE_MATCH_5}" "0-2")
if(NOT "${A}" MATCHES "Added 3 songs to alpha")
  message(FATAL_ERROR "A: no toast 'Added 3 songs to alpha' in:\n${A}")
endif()
playlist_songs(resA "${dbA}" alpha)
expect_eq("A playlist content, in list order" "${resA}" "3:a.mod;b.mod;c.mod")

# B: no saved playlist: New Playlist... creates one holding the selection.
set(dbB "${WORK}/b.db")
mp_run(B "${dbB}" queue "${select3}click ${ix} ${np_y}\nwait 2\n" ${q_execs})
if(NOT "${B}" MATCHES "song_menu_multi n=3 adds=1 list=queue ")
  message(FATAL_ERROR "B: SONGS_PLAYLIST_ADD for the selection was not sent:\n${B}")
endif()
if(NOT "${B}" MATCHES "Added 3 songs to New Playlist")
  message(FATAL_ERROR "B: no toast 'Added 3 songs to New Playlist' in:\n${B}")
endif()
playlist_songs(resB "${dbB}" "New Playlist")
expect_eq("B new playlist content" "${resB}" "3:a.mod;b.mod;c.mod")

# C: single song, no selection: New Playlist... makes a playlist of that one song.
set(dbC "${WORK}/c.db")
set(single "wait 2\nmove ${rx1} ${ry1}\nrclick ${rx1} ${ry1}\nwait 1\nclick ${ix} ${add_y}\nwait 1\nclick ${ix} ${np_y}\nwait 2\n")
mp_run(C "${dbC}" queue "${single}" ${q_execs})
if(NOT "${C}" MATCHES "Added to New Playlist")
  message(FATAL_ERROR "C: no toast 'Added to New Playlist' in:\n${C}")
endif()
playlist_songs(resC "${dbC}" "New Playlist")
expect_eq("C new playlist content" "${resC}" "1:b.mod")
message(STATUS "song_menu_multi_playlist_ok")
