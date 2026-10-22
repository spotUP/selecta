# T27b: force a relink of selecta-cli and fail when the linker log carries
# "ignoring duplicate libraries" (a static-library cycle in the link graph).
# Inputs: -DBUILD_DIR=<build dir> -DCLI=<path to selecta-cli>.
file(REMOVE "${CLI}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --target selecta-cli
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "link-warnings: relink failed (rc=${rc})\n${out}\n${err}")
endif()
if(NOT EXISTS "${CLI}")
  message(FATAL_ERROR "link-warnings: ${CLI} was not relinked")
endif()
if("${out}${err}" MATCHES "ignoring duplicate libraries")
  message(FATAL_ERROR "link-warnings: duplicate library warning in the link log\n${out}\n${err}")
endif()
message(STATUS "link-warnings: relinked selecta-cli, no duplicate library warning")
