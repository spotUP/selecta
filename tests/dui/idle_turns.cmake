# dui-idle-turns (I525): the live desktop loop idles in poll when nothing on screen changes. -DSELECTA -DSONGS -DWORK -DSDL_BUILT
# On AGA a turn that presents costs a c2p of the dirty rows plus ChangeScreenBuffer/WaitTOF, so a loop that presents on
# every turn holds the CPU at 100 %: clicks queue behind it and the other tasks (wasabid) starve. The live turn
# (plugins/ui/desktop/desktop_ui.c desk_wait_turn, shared by sdl, rtg and aga) counts where each turn returned; the stats
# line `turns idle= present= scan= input= viz= bits ...` gives the counts. This runs the real live loop on the dummy
# driver (SELECTA_SDL_TEST_LIVE=1) for 3 s with a song playing and no visualizer, and with nothing playing.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
if(NOT SDL_BUILT)
  return()
endif()
set(ENV{SDL_VIDEODRIVER} dummy)
set(ENV{SELECTA_SDL_TEST_LIVE} 1)
set(ENV{SELECTA_SDL_TEST_TIMED} "q@3000")
# Own control socket: a selecta already running on the machine must not turn these runs into "already running".
set(ENV{SELECTA_SOCKET} "${WORK}/ctl.sock")

function(turns_of out tag)
  if(NOT "${out}" MATCHES "turns idle=([0-9]+) present=([0-9]+) scan=([0-9]+) input=([0-9]+) viz=([0-9]+) bits song=([0-9]+) queue=([0-9]+) transport=([0-9]+) progress=([0-9]+)")
    message(FATAL_ERROR "${tag}: no turns line in:\n${out}")
  endif()
  set(idle ${CMAKE_MATCH_1} PARENT_SCOPE)
  set(present ${CMAKE_MATCH_2} PARENT_SCOPE)
  set(progress ${CMAKE_MATCH_9} PARENT_SCOPE)
  set(viz ${CMAKE_MATCH_5} PARENT_SCOPE)
  message(STATUS "${tag}: ${CMAKE_MATCH_0}")
endfunction()

# Nothing playing: after the first frames every turn waits in poll.
dui_run(o0 --ui desktop --backend sdl --view home --canvas 1280x800 --stats)
turns_of("${o0}" stopped)
if(present GREATER 5)
  message(FATAL_ERROR "stopped: ${present} live turns presented in 3 s with nothing changing (expected at most 5)")
endif()

# A song playing: the clock text changes once a second and the progress bar moves a pixel now and then; a turn may present
# only for those, not for every engine block. A 3 s run has about 150 engine blocks; allow the three clock seconds, the bar
# steps of a 1280 px wide bar over the test song, and the first frames.
dui_run(o1 --ui desktop --backend sdl --view home --canvas 1280x800 --stats --exec "QUEUE_ADD ${SONGS}/selecta-test.mod" --exec PLAY)
turns_of("${o1}" playing)
if(present GREATER 30)
  message(FATAL_ERROR "playing: ${present} live turns presented in 3 s (progress=${progress})")
endif()
# The visualizer strip draws one frame per engine block; each of those turns must still reach the bounded poll wait.
math(EXPR busy "${present} + ${viz}")
if(idle LESS 50 OR busy GREATER idle)
  message(FATAL_ERROR "playing: idle=${idle} present=${present} viz=${viz}: the live loop skipped the poll wait between engine blocks")
endif()
