# T19 selecta-browse-play (plan 2026-10-23-m1-1-text-ui-queue.md, Finish line).
# Drives the real shell with the scripted key file: browse into songs, move to
# selecta-test-mk.mod, Enter, wait 2000 ms, open the info panel, quit.
#   -DSELECTA=<exe> -DMOD=<selecta-test-mk.mod> -DKEYS=<keys file>
#   -DGOLDEN=<screen golden> -DWORK=<scratch dir>
# Asserts the sentinel lines (the UI ran, one song opened through the queue,
# every key was decoded) and that the last dumped screen equals the golden.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/root/songs")
file(COPY "${MOD}" DESTINATION "${WORK}/root/songs")

# K = key lines of the keys file: not blank, not a comment, not `wait`.
file(STRINGS "${KEYS}" key_lines)
set(want_keys 0)
foreach(line IN LISTS key_lines)
  string(STRIP "${line}" line)
  if(line STREQUAL "" OR line MATCHES "^#" OR line MATCHES "^wait ")
    continue()
  endif()
  math(EXPR want_keys "${want_keys} + 1")
endforeach()

if(NOT EXISTS "${SELECTA}")
  message(FATAL_ERROR "selecta shell not built: ${SELECTA}")
endif()
set(extra)
if(DEFINED BINDINGS)
  set(extra --bindings "${BINDINGS}") # I76: the same run with a rebound table
endif()
execute_process(
  COMMAND "${SELECTA}" --output null --start-dir "${WORK}/root" --keys "${KEYS}"
          --screen-dump "${WORK}/screen.txt" ${extra}
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()

if(NOT out MATCHES "ui_waits=([0-9]+) flushes=([0-9]+) keys=([0-9]+)")
  message(FATAL_ERROR "no ui_waits line\n${out}")
endif()
set(waits ${CMAKE_MATCH_1})
set(flushes ${CMAKE_MATCH_2})
set(keys ${CMAKE_MATCH_3})
if(NOT out MATCHES "songs_opened=([0-9]+) queue_version=([0-9]+)")
  message(FATAL_ERROR "no songs_opened line\n${out}")
endif()
set(opened ${CMAKE_MATCH_1})
set(qver ${CMAKE_MATCH_2})
if(waits LESS 100 OR flushes LESS 2)
  message(FATAL_ERROR "the UI barely ran: ui_waits=${waits} flushes=${flushes}")
endif()
if(NOT keys EQUAL want_keys)
  message(FATAL_ERROR "keys=${keys}, the keys file has ${want_keys}")
endif()
if(NOT opened EQUAL 1)
  message(FATAL_ERROR "songs_opened=${opened}, want 1")
endif()
if(NOT qver EQUAL 1)
  message(FATAL_ERROR "queue_version=${qver}, want 1")
endif()

file(READ "${WORK}/screen.txt" screen)
file(READ "${GOLDEN}" golden)
if(NOT screen STREQUAL golden)
  message(FATAL_ERROR "screen differs from ${GOLDEN}\nexpected:\n${golden}\ngot:\n${screen}")
endif()
message(STATUS "ui_waits=${waits} flushes=${flushes} keys=${keys} songs_opened=${opened} queue_version=${qver}")
