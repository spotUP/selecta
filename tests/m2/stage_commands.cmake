# I129 stage-commands: FILTER_LED and MIX_STEREO_WIDTH sent through the engine's command dispatch
# (render --script) reach the mixer stages: the sentinels led_frames= and width_frames= count the
# frames, and the render equals the one made with the --led / --stereo-width setter flags.
# Inputs: CLI MKMOD WORK SCRIPT
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
function(render tag)
  m2_run_ok(I129 out render "${MKMOD}" --output "${WORK}/${tag}.wav" --seconds 2 ${ARGN})
  file(SHA256 "${WORK}/${tag}.wav" h)
  set(${tag}_out "${out}" PARENT_SCOPE)
  set(${tag}_hash "${h}" PARENT_SCOPE)
endfunction()
render(plain)
render(cmd --script "${SCRIPT}")
render(flags --led on --stereo-width 50)
m2_check(I129 "${cmd_out}" "led==on,led_frames>0,width_frames>0")
if(NOT cmd_hash STREQUAL flags_hash)
  message(FATAL_ERROR "RED[I129]: the command path render differs from the setter render")
endif()
if(cmd_hash STREQUAL plain_hash)
  message(FATAL_ERROR "RED[I129]: the commands did not move the output hash")
endif()
