# dui-overlays (DUI-P4-13): the command palette, the shortcut help, the `:` command line with completion (F45), the song
# context menu and toasts, through the real shell and the scripted --input feeder.
#   -DSELECTA -DSONGS -DWORK
# The desktop dumps an `overlay ...` line in its stats: which overlay is open, how often each opened and ran, the verb it
# sent last, the toast state, the per-item menu picks and the queue size. Every check reads that line, so a feature that
# is not wired to the keys or the pointer leaves the counters at zero and fails.
# 1. Ctrl+P opens the palette; the query `shuff` ranks SHUFFLE first, and Enter on it (it needs an argument) opens the
#    command line with `SHUFFLE ` typed, so `on` and Enter send `SHUFFLE on`. Down then Enter picks RESHUFFLE, a bare
#    verb, which runs straight from the palette.
# 2. `:` opens the command line; `pla` and Tab list the PLAY and PLAYLIST_* verbs of the action list.
# 3. `?` opens the help overlay; Escape closes any overlay.
# 4. The song context menu fires each of its four items; Remove drops one queue entry; the toast shows, then expires.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
foreach(n banana apple cherry)
  configure_file("${SONGS}/selecta-test.mod" "${WORK}/${n}.mod" COPYONLY)
endforeach()

function(expect_eq what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "${what}: got '${got}', expected '${want}'")
  endif()
endfunction()

# ov_run(<tag> <view> <script>) sets <tag>_<field> for every key=value of the overlay line and <tag>_out.
function(ov_run tag view input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${input}")
  dui_run(o --ui desktop --backend mem --view ${view} --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${WORK}/banana.mod" --exec "QUEUE_ADD ${WORK}/apple.mod" --exec "QUEUE_ADD ${WORK}/cherry.mod"
    --input "${WORK}/${tag}.txt")
  if(NOT "${o}" MATCHES "overlay kind=")
    message(FATAL_ERROR "no overlay line in:\n${o}")
  endif()
  # CMake keeps only nine match groups, so each field gets its own match.
  foreach(f kind palette_opens palette_runs help_opens cmdline_opens cmdline_runs sends_ok toasts toast_on pal_count queue)
    string(REGEX MATCH "overlay [^\n]*[ ]${f}=([0-9-]+)" _m "${o}")
    if(f STREQUAL "kind")
      string(REGEX MATCH "overlay kind=([0-9-]+)" _m "${o}")
    elseif(f STREQUAL "sends_ok")
      string(REGEX MATCH "[ ]sends_ok=([0-9]+)" _m "${o}")
    endif()
    set(${tag}_${f} "${CMAKE_MATCH_1}" PARENT_SCOPE)
  endforeach()
  string(REGEX MATCH "menu_picks=([0-9]+),([0-9]+),([0-9]+),([0-9]+)" _m "${o}")
  set(${tag}_pick0 "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_pick1 "${CMAKE_MATCH_2}" PARENT_SCOPE)
  set(${tag}_pick2 "${CMAKE_MATCH_3}" PARENT_SCOPE)
  set(${tag}_pick3 "${CMAKE_MATCH_4}" PARENT_SCOPE)
  string(REGEX MATCH "last_verb=([^\n]*) pal_count=" _m "${o}")
  set(${tag}_last_verb "${CMAKE_MATCH_1}" PARENT_SCOPE)
  string(REGEX MATCH "status=([^\n]*)" _m "${o}")
  set(${tag}_status "${CMAKE_MATCH_1}" PARENT_SCOPE)
  string(REGEX MATCH "palitem 0 ([^\n]*)" _p "${o}")
  set(${tag}_first "${CMAKE_MATCH_1}" PARENT_SCOPE)
  set(${tag}_out "${o}" PARENT_SCOPE)
endfunction()

set(shuff "key ctrl+p\nkey s\nkey h\nkey u\nkey f\nkey f\n")

# 1. palette: ranking, the argument verb through the command line, a bare verb straight from the palette
ov_run(p queue "wait 100\n${shuff}")
expect_eq("palette open" "${p_kind}" 1)
expect_eq("palette opens" "${p_palette_opens}" 1)
expect_eq("shuff ranks first" "${p_first}" "SHUFFLE")
ov_run(s queue "wait 100\n${shuff}key enter\nkey o\nkey n\nkey enter\nwait 100\n")
expect_eq("overlay closed after the command" "${s_kind}" 0)
expect_eq("SHUFFLE needs its argument: command line runs" "${s_cmdline_runs}" 1)
expect_eq("line sent" "${s_last_verb}" "SHUFFLE on")
expect_eq("engine took the line" "${s_sends_ok}" 1)
ov_run(r queue "wait 100\n${shuff}key down\nkey enter\nwait 100\n")
expect_eq("bare verb runs from the palette" "${r_palette_runs}" 1)
expect_eq("verb sent" "${r_last_verb}" "RESHUFFLE")
expect_eq("engine took the verb" "${r_sends_ok}" 1)
ov_run(e queue "wait 100\n${shuff}key escape\n")
expect_eq("Escape closes the palette" "${e_kind}" 0)
expect_eq("Escape sends nothing" "${e_sends_ok}" 0)

# 2. command line completion
ov_run(c queue "wait 100\nkey :\nkey p\nkey l\nkey a\nkey tab\n")
expect_eq("command line open" "${c_kind}" 3)
expect_eq("command line opens" "${c_cmdline_opens}" 1)
foreach(v PLAY PLAYLIST_ADD_SONG PLAYLIST_RENAME PLAYLIST_DEL)
  if(NOT "${c_status}" MATCHES "(^| )${v}( |$)")
    message(FATAL_ERROR "completion of 'pla' misses ${v}: ${c_status}")
  endif()
endforeach()
ov_run(l queue "wait 100\nkey :\nkey p\nkey l\nkey a\nkey y\nkey enter\nwait 100\n")
expect_eq("typed line runs" "${l_cmdline_runs}" 1)
expect_eq("typed line" "${l_last_verb}" "play")

# 3. help overlay
ov_run(h queue "wait 100\nkey ?\n")
expect_eq("help open" "${h_kind}" 2)
expect_eq("help opens" "${h_help_opens}" 1)

# 4. context menu: each item fires, Remove drops one entry, the toast shows then expires
foreach(k 0 1 2 3)
  math(EXPR my "142 + ${k} * 24 + 12")
  ov_run(m${k} songs "wait 100\nmove 600 110\nrclick 600 142\nclick 610 ${my}\n")
  expect_eq("menu item ${k} picked" "${m${k}_pick${k}}" 1)
  expect_eq("menu item ${k} toast" "${m${k}_toast_on}" 1)
endforeach()
expect_eq("Remove drops one entry" "${m3_queue}" 2)
expect_eq("Add to Queue adds one entry" "${m2_queue}" 4)
ov_run(x songs "wait 100\nmove 600 110\nrclick 600 142\nclick 610 154\nwait 3000\n")
expect_eq("toast expires" "${x_toast_on}" 0)
expect_eq("toast was shown" "${x_toasts}" 1)
