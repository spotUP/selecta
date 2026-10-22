# T10 / T11 driver: runs `selecta-cli machine` (host, or the 68k binary under
# vamos) and checks the probe output. Inputs:
#   -DCLI          host CLI (T10) or -DVAMOS and -DAMIGA_CLI (T11)
#   -DTAGS_DEF     include/selecta/tags.def, from which the expected rows are counted
#   -DPLATFORM       host | amiga
#   -DEXPECT_CPU -DEXPECT_FPU -DEXPECT_TIER
#   -DEXPECT_FAST_RAM, -DEXPECT_CHIP_RAM  numbers; or EXPECT_FAST_RAM=sysctl
#                  (host: read `sysctl -n hw.memsize` or /proc/meminfo now)
foreach(v TAGS_DEF PLATFORM EXPECT_CPU EXPECT_FPU EXPECT_TIER)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "machine.cmake: -D${v} is required")
  endif()
endforeach()

if(PLATFORM STREQUAL "amiga")
  execute_process(COMMAND "${VAMOS}" -C 68020 -- "${AMIGA_CLI}" machine
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
else()
  execute_process(COMMAND "${CLI}" machine
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "selecta-cli machine exited ${rc}\nstdout:\n${out}\nstderr:\n${err}")
endif()

# The sentinel: the probe's own call counter, exactly one probe.
if(NOT out MATCHES "(^|\n)probe_calls=1\n")
  message(FATAL_ERROR "missing probe_calls=1\n${out}")
endif()

# Every SEL_MACHINE_* row of tags.def is printed exactly once as NAME=value.
file(STRINGS "${TAGS_DEF}" rows REGEX "^SEL_TAG\\(MACHINE,")
list(LENGTH rows nrows)
if(nrows EQUAL 0)
  message(FATAL_ERROR "no SEL_MACHINE rows in ${TAGS_DEF}")
endif()
foreach(row IN LISTS rows)
  string(REGEX REPLACE "^SEL_TAG\\(MACHINE, *([A-Z_]+),.*$" "\\1" name "${row}")
  string(REGEX MATCHALL "(^|\n)${name}=[^\n]*" hits "${out}")
  list(LENGTH hits n)
  if(NOT n EQUAL 1)
    message(FATAL_ERROR "${name} printed ${n} times, expected once\n${out}")
  endif()
endforeach()

function(sel_get name outvar)
  string(REGEX MATCH "(^|\n)${name}=([^\n]*)" m "${out}")
  set(${outvar} "${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

if(EXPECT_FAST_RAM STREQUAL "sysctl")
  if(APPLE)
    execute_process(COMMAND sysctl -n hw.memsize OUTPUT_VARIABLE EXPECT_FAST_RAM
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  else()
    file(STRINGS /proc/meminfo memline REGEX "^MemTotal:")
    string(REGEX REPLACE "^MemTotal: *([0-9]+) kB.*$" "\\1" kb "${memline}")
    math(EXPR EXPECT_FAST_RAM "${kb} * 1024")
  endif()
endif()

set(expected CPU=${EXPECT_CPU} FPU=${EXPECT_FPU} TIER=${EXPECT_TIER} FAST_RAM=${EXPECT_FAST_RAM})
if(DEFINED EXPECT_CHIP_RAM)
  list(APPEND expected CHIP_RAM=${EXPECT_CHIP_RAM})
endif()
foreach(pair IN LISTS expected)
  string(REGEX REPLACE "=.*$" "" name "${pair}")
  string(REGEX REPLACE "^[^=]*=" "" want "${pair}")
  sel_get(${name} got)
  if(NOT got STREQUAL want)
    message(FATAL_ERROR "${name}: expected ${want}, got '${got}'\n${out}")
  endif()
endforeach()
message(STATUS "[OK] machine probe: ${nrows} rows, ${expected}")
