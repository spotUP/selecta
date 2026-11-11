# T5 negative path: with the broken module on $SELECTA_PLUGIN_PATH, plugins --lint must count one unknown tag,
# name it, and fail. Inputs: CLI (selecta-cli), DIR (directory holding sel_bad), EXPECT_PLUGINS (count incl. bad).
if(NOT EXISTS "${DIR}")
  message(FATAL_ERROR "bad-plugin module directory missing: ${DIR}")
endif()
set(ENV{SELECTA_PLUGIN_PATH} "${DIR}")
execute_process(
  COMMAND "${CLI}" plugins --lint
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
