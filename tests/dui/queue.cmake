# dui-queue (DUI-P4-8): the desktop Queue page, through the real shell and the scripted --input feeder.
#   -DSELECTA -DSONGS -DWORK -DGOLDEN
# Four copies of the test module are queued (banana, apple, cherry, dates). The page dumps its own geometry (qbtn, qrow,
# qnext, qdel lines) and a queue hash (FNV over id, flags and path of every entry in order), so the clicks below use the
# dumped rectangles of a first, untouched run and nothing is hard-coded.
# 1. Play next then Undo restores the queue exactly: hash before equals hash after (the plan line), Redo brings the edit
#    back, and the history depths in the dump follow.
# 2. Drag reorder moves row 0 to row 3; Remove drops one entry and Undo restores it.
# 3. Scope cycles library -> album, Reshuffle sends its verb; a loaded .prg playlist shows its divider as a group row.
# 4. Sentinel `queue draws` and `cmds` prove the page drew and the pointer code sent each verb; a golden pins the frame.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n banana apple cherry dates three four)
  configure_file("${SONGS}/selecta-test.mod" "${WORK}/${n}.mod" COPYONLY)
endforeach()
configure_file("${HERE}/../m12/list.prg" "${WORK}/list.prg" COPYONLY)

# queue_run(<tag> <input script> [extra dui_run args]) sets <tag>_hash, _count, _undo, _redo, _scope, _cmds, _draws and the
# raw text <tag>_out.
function(queue_run tag input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view queue --canvas 1280x800 --clock fixed --stats ${ARGN} --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "queue draws ([0-9]+) rows ([0-9]+) cmds ([0-9]+) hash ([0-9a-f]+) count ([0-9]+) undo ([0-9]+) redo ([0-9]+) scope ([a-z]+)")
    message(FATAL_ERROR "no queue sentinel in:\n${o}")
  endif()
  set(${tag}_draws "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_cmds "${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(${tag}_hash "${CMAKE_MATCH_4}" PARENT_SCOPE)
  set(${tag}_count "${CMAKE_MATCH_5}" PARENT_SCOPE)
  set(${tag}_undo "${CMAKE_MATCH_6}" PARENT_SCOPE)
  set(${tag}_redo "${CMAKE_MATCH_7}" PARENT_SCOPE)
  set(${tag}_scope "${CMAKE_MATCH_8}" PARENT_SCOPE)
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

set(adds --exec "QUEUE_ADD ${WORK}/banana.mod" --exec "QUEUE_ADD ${WORK}/apple.mod" --exec "QUEUE_ADD ${WORK}/cherry.mod"
  --exec "QUEUE_ADD ${WORK}/dates.mod")

queue_run(a "wait 100\n" ${adds})
expect_eq("queued entries" "${a_count}" 4)
set(base "${a_undo}") # the four QUEUE_ADD verbs are undoable edits too
if(base LESS 1)
  message(FATAL_ERROR "the queue edits are not in the undo history (undo ${base})")
endif()
math(EXPR base1 "${base} + 1")
if(a_draws LESS 1)
  message(FATAL_ERROR "the Queue page did not draw (queue draws ${a_draws})")
endif()
centre_of(undo_xy "${a_out}" qbtn 0)
centre_of(redo_xy "${a_out}" qbtn 1)
centre_of(resh_xy "${a_out}" qbtn 2)
centre_of(scope_xy "${a_out}" qbtn 3)
centre_of(next2 "${a_out}" qnext 2)
centre_of(del1 "${a_out}" qdel 1)
centre_of(row0 "${a_out}" qrow 0)
centre_of(row3 "${a_out}" qrow 3)
string(REGEX MATCH "qpath 0 ([^\n]*)" _p "${a_out}")
set(path0 "${CMAKE_MATCH_1}")

# 1. play next, then undo: the queue hash is restored exactly
queue_run(n "click ${next2}\nwait 100\n" ${adds})
if(n_hash STREQUAL a_hash)
  message(FATAL_ERROR "Play Next did not change the queue (hash ${n_hash})")
endif()
expect_eq("undo depth after Play Next" "${n_undo}" "${base1}")
expect_eq("verbs sent for Play Next" "${n_cmds}" 1)
queue_run(u "click ${next2}\nwait 100\nclick ${undo_xy}\nwait 100\n" ${adds})
expect_eq("queue hash after Play Next then Undo" "${u_hash}" "${a_hash}")
expect_eq("undo depth after Undo" "${u_undo}" "${base}")
expect_eq("redo depth after Undo" "${u_redo}" 1)
queue_run(r "click ${next2}\nwait 100\nclick ${undo_xy}\nwait 100\nclick ${redo_xy}\nwait 100\n" ${adds})
expect_eq("queue hash after Play Next, Undo, Redo" "${r_hash}" "${n_hash}")
expect_eq("redo depth after Redo" "${r_redo}" 0)
expect_eq("verbs sent for Play Next, Undo, Redo" "${r_cmds}" 3)

# 2. drag reorder and remove
queue_run(m "drag ${row0} ${row3}\nwait 100\n" ${adds})
if(m_hash STREQUAL a_hash)
  message(FATAL_ERROR "the drag did not reorder the queue")
endif()
expect_eq("entries after a reorder" "${m_count}" 4)
string(REGEX MATCH "qpath 3 ([^\n]*)" _p "${m_out}")
expect_eq("the dragged entry is last" "${CMAKE_MATCH_1}" "${path0}")
queue_run(d "click ${del1}\nwait 100\n" ${adds})
expect_eq("entries after Remove" "${d_count}" 3)
queue_run(du "click ${del1}\nwait 100\nclick ${undo_xy}\nwait 100\n" ${adds})
expect_eq("queue hash after Remove then Undo" "${du_hash}" "${a_hash}")

# 3. scope and reshuffle
queue_run(s "click ${scope_xy}\nwait 100\n" ${adds})
expect_eq("scope after one click" "${s_scope}" album)
queue_run(h "click ${resh_xy}\nwait 100\n" ${adds})
expect_eq("verbs sent for Reshuffle" "${h_cmds}" 1)
expect_eq("entries after Reshuffle" "${h_count}" 4)

# groups: the divider of a HiPPrg playlist is a group row
queue_run(g "wait 100\n" --exec "PLAYLIST_LOAD ${WORK}/list.prg")
if(NOT "${g_out}" MATCHES "qrow 1 [0-9]+ [0-9]+ [0-9]+ [0-9]+ divider")
  message(FATAL_ERROR "the .prg divider is not a group row in:\n${g_out}")
endif()

set(gfile "${GOLDEN}/desktop/queue-rgba32-1280x800@1.hash")
if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
  file(WRITE "${gfile}" "${a_canvas}\n")
endif()
if(NOT EXISTS "${gfile}")
  message(FATAL_ERROR "missing golden ${gfile} (hash ${a_canvas}); create it with SELECTA_GOLDEN_UPDATE=1")
endif()
file(STRINGS "${gfile}" want LIMIT_COUNT 1)
expect_eq("queue frame hash vs golden ${gfile}" "${a_canvas}" "${want}")
