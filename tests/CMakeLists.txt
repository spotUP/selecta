# ctest entries T1 to T9 (plan 2026-10-22-m0-skeleton.md). Expected counts
# come from cmake/selecta_defs.cmake, never retyped here.
set(SEL_CLI $<TARGET_FILE:selecta-cli>)
set(SEL_EXPECT "${CMAKE_CURRENT_SOURCE_DIR}/expect.cmake")
math(EXPR SEL_COUNT_MSG "${SEL_COUNT_CMD} + ${SEL_COUNT_EVT}")

function(sel_cli_test name args)
  # Remaining arguments are -D settings for expect.cmake.
  add_test(NAME ${name}
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DARGS=${args}" ${ARGN} -P "${SEL_EXPECT}")
endfunction()

# T1: every row of both tables survives encode -> decode.
sel_cli_test(protocol-roundtrip "roundtrip"
  "-DEXPECT=commands=${SEL_COUNT_CMD} events=${SEL_COUNT_EVT} ok=${SEL_COUNT_MSG}")

# T2: reflection lists one line per table row.
sel_cli_test(protocol-reflection-commands "commands" "-DLINES=${SEL_COUNT_CMD}")
sel_cli_test(protocol-reflection-events   "events"   "-DLINES=${SEL_COUNT_EVT}")
sel_cli_test(protocol-reflection-tagtypes "tagtypes" "-DLINES=${SEL_COUNT_META}")

# T5: the registry walked every descriptor and found only known tags.
sel_cli_test(registry-tags "plugins --lint" "-DEXPECT=plugins=1 unknown_tags=0 duplicate_tags=0")

# T6: sel_host layout golden per architecture.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SEL_ARCH)
sel_cli_test(abi-host-layout "abi" "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/abi/sel_host.${SEL_ARCH}.txt")
