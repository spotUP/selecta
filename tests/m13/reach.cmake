# T39 selecta-m13-reach (plan 2026-10-23-m1-3-viz-voices-playback.md, Phase 6).
# The M1.3 reachability test: the real shell, scripted keys, one run that plays selecta-test.mod
# (nuade, voice kind), shows the pattern and viz panels, mutes voice 1, sets the crossfeed DSP
# and arms the sleep timer. The shell prints
#   viz_frames=V scope_reads=S pattern_rows=P mutes=M dsp=crossfeed:on fades=F sleep_armed=1
# on exit; every number comes from the code that did the work.
#   -DSELECTA=<exe> -DMOD=<selecta-test.mod> -DKEYS=<keys> -DGOLDEN=<screen golden> -DWORK=<scratch dir>
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/root/songs" "${WORK}/config")
file(COPY "${MOD}" DESTINATION "${WORK}/root/songs")
if(NOT EXISTS "${SELECTA}")
  message(FATAL_ERROR "selecta shell not built: ${SELECTA}")
endif()
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}/root" --config-dir "${WORK}/config"
          --keys "${KEYS}" --screen-dump "${WORK}/screen.txt"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()

set(re "viz_frames=([0-9]+) scope_reads=([0-9]+) pattern_rows=([0-9]+) mutes=([0-9]+) dsp=([a-z:]+) fades=([0-9]+) sleep_armed=([0-9]+)")
if(NOT out MATCHES "${re}")
  message(FATAL_ERROR "no viz_frames line\nstdout:\n${out}\nstderr:\n${err}")
endif()
set(viz_frames ${CMAKE_MATCH_1})
set(scope_reads ${CMAKE_MATCH_2})
set(pattern_rows ${CMAKE_MATCH_3})
set(got "mutes=${CMAKE_MATCH_4} dsp=${CMAKE_MATCH_5} sleep_armed=${CMAKE_MATCH_7}")
if(viz_frames LESS 1 OR scope_reads LESS 1 OR pattern_rows LESS 1)
  message(FATAL_ERROR "counters must be > 0: viz_frames=${viz_frames} scope_reads=${scope_reads} pattern_rows=${pattern_rows}")
endif()
set(want "mutes=1 dsp=crossfeed:on sleep_armed=1")
if(NOT got STREQUAL want)
  message(FATAL_ERROR "counters\n  got:  ${got}\n  want: ${want}")
endif()

file(READ "${WORK}/screen.txt" screen)
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "${viz_frames} ${scope_reads} ${pattern_rows} ${got}")
