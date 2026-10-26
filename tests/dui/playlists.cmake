# dui-playlists (DUI-P4-9, with I241): the desktop Playlists page, through the real shell and the scripted --input feeder.
#   -DSELECTA -DSONGS -DWORK -DGOLDEN
# The page dumps its own geometry (plbtn, plname, plslot lines) and its rows (plrow lines), so every click below uses the
# rectangles of a first, untouched run and nothing is hard-coded. Runs that share PLDB share one library file, so a later
# run is a reload of what an earlier one stored.
# 1. Create a folder, select it, create a playlist in it, then reload the library: the folder holds the playlist (the
#    plan line) and the reload counted one folder open (STAT_FOLDER_OPENS).
# 2. Rename, delete with a confirm (Cancel keeps it), and drag a playlist onto another folder.
# 3. I241: on the Songs page a drag of one song row onto a playlist target adds exactly one entry to that playlist.
# 4. Sentinel `pl draws` and `cmds` prove the page drew and the pointer code sent each verb; a golden pins the frame.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n one two three)
  configure_file("${SONGS}/selecta-test.mod" "${WORK}/${n}.mod" COPYONLY)
endforeach()

# pl_run(<tag> <input script> [extra dui_run args]) runs the Playlists page on the library file PLDB and sets <tag>_out,
# _draws, _rows, _cmds, _opens, _confirm, _queue and _canvas.
function(pl_run tag input)
  file(WRITE "${WORK}/${tag}.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view playlists --canvas 1280x800 --clock fixed --stats
    --db "${PLDB}" ${ARGN} --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "pl draws ([0-9]+) rows ([0-9]+) cmds ([0-9]+) opens ([0-9]+) count ([0-9]+) hash ([0-9a-f]+) sel ([0-9]+) confirm ([0-9]+) queue ([0-9]+)")
    message(FATAL_ERROR "no pl sentinel in:\n${o}")
  endif()
  set(${tag}_draws "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_rows "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_cmds "${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(${tag}_confirm "${CMAKE_MATCH_8}" PARENT_SCOPE)
  set(${tag}_queue "${CMAKE_MATCH_9}" PARENT_SCOPE)
  if(NOT "${o}" MATCHES "folders_screen opens=([0-9]+)")
    message(FATAL_ERROR "no folders_screen opens in:\n${o}")
  endif()
  set(${tag}_opens "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_out "${o}" PARENT_SCOPE)
  dui_stat(h "${o}" desktop canvas_hash)
  set(${tag}_canvas "${h}" PARENT_SCOPE)
endfunction()

# centre_of(<out var> <dump> <key> <index>) is "x y", the centre of the dumped rectangle "<key> <index> x y w h".
function(centre_of outvar dump key idx)
  if(NOT "${dump}" MATCHES "${key} ${idx} ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)")
    message(FATAL_ERROR "no ${key} ${idx} geometry in:\n${dump}")
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

# type_text(<out var> <word>) is the script lines that type a lowercase word into the name field.
function(type_text outvar word)
  set(s "")
  string(LENGTH "${word}" n)
  math(EXPR last "${n} - 1")
  foreach(i RANGE 0 ${last})
    string(SUBSTRING "${word}" ${i} 1 c)
    string(APPEND s "key ${c}\n")
  endforeach()
  set(${outvar} "${s}" PARENT_SCOPE)
endfunction()

set(PLDB "${WORK}/lib.db")
pl_run(a "wait 100\n")
if(a_draws LESS 1)
  message(FATAL_ERROR "the Playlists page did not draw (pl draws ${a_draws})")
endif()
expect_eq("rows in a fresh library" "${a_rows}" 0)
expect_eq("verbs sent by an idle page" "${a_cmds}" 0)
# I364: an empty list draws its one-line empty state; the heading is drawn by the same frame (draws > 0).
if(NOT "${a_out}" MATCHES "\nempty No playlists yet. Type a name and choose New Playlist.\n")
  message(FATAL_ERROR "an empty Playlists page must draw its empty state in:\n${a_out}")
endif()
foreach(i 0 1 2 3 4)
  centre_of(btn${i} "${a_out}" plbtn ${i})
endforeach()
centre_of(name "${a_out}" plname 0)
centre_of(slot0 "${a_out}" plslot 0)
centre_of(slot1 "${a_out}" plslot 1)
type_text(t_mix mix)
type_text(t_kit kit)
type_text(t_box box)
type_text(t_cab cab)
type_text(t_new new)

# 1. create a folder, select it, create a playlist inside it (the queue's entry goes into the playlist), then reload
pl_run(c "click ${name}\n${t_mix}click ${btn0}\nwait 100\nclick ${slot0}\nwait 100\nclick ${name}\n${t_kit}click ${btn1}\nwait 100\n"
  --exec "QUEUE_ADD ${WORK}/one.mod")
expect_eq("verbs sent for two creates" "${c_cmds}" 2)
pl_run(r "wait 100\n")
if(NOT "${r_out}" MATCHES "plrow 0 folder [0-9]+ 0 mix")
  message(FATAL_ERROR "the folder 'mix' is not row 0 after a reload in:\n${r_out}")
endif()
if(NOT "${r_out}" MATCHES "plrow 1 playlist [0-9]+ 1 kit")
  message(FATAL_ERROR "the folder 'mix' does not hold the playlist 'kit' after a reload in:\n${r_out}")
endif()
expect_eq("rows after reload" "${r_rows}" 2)
if(r_opens LESS 1)
  message(FATAL_ERROR "the reload did not count a folder open (opens ${r_opens})")
endif()
# Play loads the playlist into the queue: one entry
pl_run(p "click ${slot1}\nwait 100\nclick ${btn4}\nwait 100\n")
expect_eq("queue entries after Play" "${p_queue}" 1)

# 2. rename (needs a typed name), cancel a delete
pl_run(n "click ${slot1}\nwait 100\nclick ${name}\n${t_cab}click ${btn2}\nwait 100\n")
if(NOT "${n_out}" MATCHES "plrow 1 playlist [0-9]+ 1 cab")
  message(FATAL_ERROR "Rename did not rename the playlist in:\n${n_out}")
endif()
pl_run(dc "click ${slot1}\nwait 100\nclick ${btn3}\nwait 100\n")
expect_eq("confirm pending after Delete" "${dc_confirm}" 1)
centre_of(cancel "${dc_out}" plbtn 2)
centre_of(confirm "${dc_out}" plbtn 3)
pl_run(dk "click ${slot1}\nwait 100\nclick ${btn3}\nwait 100\nclick ${cancel}\nwait 100\n")
expect_eq("rows after Cancel" "${dk_rows}" 2)
expect_eq("confirm pending after Cancel" "${dk_confirm}" 0)

# drag a playlist onto another folder: a second folder, then drag the playlist onto it
pl_run(f "click ${name}\n${t_box}click ${btn0}\nwait 100\n")
expect_eq("rows with a second folder" "${f_rows}" 3)
if(NOT "${f_out}" MATCHES "plrow ([0-9]) folder [0-9]+ 0 box")
  message(FATAL_ERROR "the folder 'box' is not on the page in:\n${f_out}")
endif()
centre_of(boxslot "${f_out}" plslot ${CMAKE_MATCH_1})
pl_run(m "click ${slot1}\nwait 100\ndrag ${slot1} ${boxslot}\nwait 100\n")
pl_run(m2 "wait 100\n")
if(NOT "${m2_out}" MATCHES "plrow ([0-9]) folder [0-9]+ 0 box\nplrow ([0-9]) playlist [0-9]+ 1 cab")
  message(FATAL_ERROR "the playlist is not under 'box' after the move and a reload in:\n${m2_out}")
endif()

# delete with a confirm removes the playlist, and the reload agrees
centre_of(cabslot "${m2_out}" plslot ${CMAKE_MATCH_2})
pl_run(x "click ${cabslot}\nwait 100\nclick ${btn3}\nwait 100\nclick ${confirm}\nwait 100\n")
expect_eq("rows after a confirmed Delete" "${x_rows}" 2)
pl_run(x2 "wait 100\n")
expect_eq("rows after Delete and a reload" "${x2_rows}" 2)

# 3. I241: a fresh library with one playlist holding one entry; on the Songs page a drag of one song row onto the first
# playlist target (below Queue in the sidebar) appends exactly one entry to it. Play on the Playlists page reads it back.
set(PLDB "${WORK}/drop.db")
pl_run(s0 "click ${name}\n${t_new}click ${btn1}\nwait 100\n" --exec "QUEUE_ADD ${WORK}/one.mod")
pl_run(s1 "click ${slot0}\nwait 100\nclick ${btn4}\nwait 100\n")
expect_eq("entries of the new playlist before the drop" "${s1_queue}" 1)
file(WRITE "${WORK}/drop.txt" "drag 600 142 50 90\nwait 100\n")
dui_run(drop --ui desktop --backend mem --view songs --canvas 1280x800 --clock fixed --stats --db "${PLDB}"
  --exec "QUEUE_ADD ${WORK}/one.mod" --exec "QUEUE_ADD ${WORK}/two.mod" --exec "QUEUE_ADD ${WORK}/three.mod"
  --input "${WORK}/drop.txt")
if(NOT "${drop}" MATCHES "drags=1")
  message(FATAL_ERROR "the drop on the playlist target was not counted in:\n${drop}")
endif()
pl_run(s2 "click ${slot0}\nwait 100\nclick ${btn4}\nwait 100\n")
expect_eq("entries of the playlist after one dropped song" "${s2_queue}" 2)

# 4. the golden of the untouched page
set(gfile "${GOLDEN}/desktop/playlists-rgba32-1280x800@1.hash")
if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
  file(WRITE "${gfile}" "${a_canvas}\n")
endif()
if(NOT EXISTS "${gfile}")
  message(FATAL_ERROR "missing golden ${gfile} (hash ${a_canvas}); create it with SELECTA_GOLDEN_UPDATE=1")
endif()
file(STRINGS "${gfile}" want LIMIT_COUNT 1)
expect_eq("playlists frame hash vs golden ${gfile}" "${a_canvas}" "${want}")
