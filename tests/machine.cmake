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

include("${CMAKE_CURRENT_LIST_DIR}/amiga/vamos_cmd.cmake")
sel_vamos_cmd(SEL_VAMOS_CMD "${AMIGA_CLI}")
if(PLATFORM STREQUAL "amiga")
  execute_process(COMMAND ${SEL_VAMOS_CMD} -- "${AMIGA_CLI}" machine
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

if(EXPECT_FAST_RAM STREQUAL "vamos-image")
  # vamos AvailMem(MEMF_FAST) is the free allocator total: the -m 8192 RAM
  # (8388608) less a constant 307704 of runtime use less every hunk the loader
  # allocates. The hunk sizes come from the HUNK_HEADER (0x3F3) of the binary
  # under test. The constant is not the image size: it is identical on images
  # of 3.66 MB and 5.24 MB, at any exe path and cwd (I1305k). It is:
  #   307260  vamos itself plus selecta's startup state (libnix pools, 128 KiB
  #           swapped stack, stdio), measured on 8faf9b3 and 99e18bb5;
  #   +  284  mathieeesingtrans.library, which libnix auto-opens at startup
  #           once the image references expf/powf/sqrtf (libm.a expf.o, powf.o,
  #           sqrtf.o need _MathIeeeSingTransBase). Its vamos base is 166 +
  #           name strings 28 + 44 + vectors 44 bytes. Pulled in by klystron
  #           cyd.c sqrtf (2fa40df2) and Krakatoa krk_core.c / krk_lava_fx.c
  #           expf/powf (64d87fcf, 3e158b13); the double library was already
  #           open (hively sin, sqlite log);
  #   +  160  the -V selbin: volume (80) and the -a PROGDIR: assign (80) that
  #           sel_vamos_cmd adds (9935e25e), measured with and without each.
  # A startup library the image newly references moves this number; check with
  # `vamos -l mem_alloc:info` which allocations are live at AvailMem.
  file(READ "${AMIGA_CLI}" hdr HEX LIMIT 4096)
  string(SUBSTRING "${hdr}" 0 8 magic)
  if(NOT magic STREQUAL "000003f3")
    message(FATAL_ERROR "${AMIGA_CLI} does not start with HUNK_HEADER: ${magic}")
  endif()
  # skip magic and the resident-library list end (0), then table size, first, last
  string(SUBSTRING "${hdr}" 24 8 first_hex)
  string(SUBSTRING "${hdr}" 32 8 last_hex)
  math(EXPR first "0x${first_hex}")
  math(EXPR last "0x${last_hex}")
  set(image_bytes 0)
  foreach(i RANGE ${first} ${last})
    math(EXPR off "40 + (${i} - ${first}) * 8")
    string(SUBSTRING "${hdr}" ${off} 8 sz_hex)
    math(EXPR words "0x${sz_hex} & 0x3fffffff")
    math(EXPR image_bytes "${image_bytes} + ${words} * 4")
  endforeach()
  math(EXPR EXPECT_FAST_RAM "8388608 - 307704 - ${image_bytes}")
endif()

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
