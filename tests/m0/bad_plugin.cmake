# T5 negative path: configure and build a second tree with the broken
# plugin, then require plugins --lint to count one unknown tag and fail.
# Inputs: SRC (source dir), BIN (scratch build dir), GEN (generator).
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${SRC}" -B "${BIN}" -G "${GEN}"
    -DSEL_TEST_BAD_PLUGIN=ON -DSEL_BUILD_TESTS=OFF
  RESULT_VARIABLE rc OUTPUT_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "configure of the bad-plugin tree failed: ${rc}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BIN}" --target selecta-cli
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "build of the bad-plugin tree failed:\n${out}")
endif()
execute_process(
  COMMAND "${BIN}/apps/selecta-cli/selecta-cli" plugins --lint
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
string(STRIP "${out}" out)
if(rc EQUAL 0)
  message(FATAL_ERROR "plugins --lint passed with a bad plugin: ${out}")
endif()
if(NOT out STREQUAL "plugins=${EXPECT_PLUGINS} unknown_tags=1 duplicate_tags=0")
  message(FATAL_ERROR "unexpected lint line: '${out}' (stderr: ${err})")
endif()
if(NOT err MATCHES "plugin bad: unknown tag 0x2FFF")
  message(FATAL_ERROR "lint did not name the bad tag; stderr: ${err}")
endif()
