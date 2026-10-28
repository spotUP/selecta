# format: clang-format -i over every source and header.
# lint:   -Werror build (all), sdk-isolation, and clang-tidy when present.
# The clang tools are optional on a developer machine; the build never
# fails because they are missing, it says what to install and skips.

find_program(SEL_CLANG_FORMAT clang-format)
find_program(SEL_CLANG_TIDY clang-tidy)

set(SEL_SOURCE_GLOBS
  "${PROJECT_SOURCE_DIR}/include/selecta/*.h"
  "${PROJECT_SOURCE_DIR}/src/*/*.c" "${PROJECT_SOURCE_DIR}/src/*/*.h"
  "${PROJECT_SOURCE_DIR}/plugins/*/*/*.c" "${PROJECT_SOURCE_DIR}/plugins/*/*/*.h"
  "${PROJECT_SOURCE_DIR}/apps/*/*.c" "${PROJECT_SOURCE_DIR}/apps/*/*.h"
  "${PROJECT_SOURCE_DIR}/tests/*/*.c")
file(GLOB SEL_ALL_SOURCES ${SEL_SOURCE_GLOBS})

if(SEL_CLANG_FORMAT)
  add_custom_target(format
    COMMAND "${SEL_CLANG_FORMAT}" -i ${SEL_ALL_SOURCES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "clang-format over all sources")
else()
  add_custom_target(format
    COMMAND "${CMAKE_COMMAND}" -E echo "[INFO] clang-format not found. Install with brew install clang-format, then reconfigure" VERBATIM
    COMMENT "clang-format missing")
endif()

# "all" is a phony target under Ninja only; Makefile generators read DEPENDS all as a file and stop with "No rule to make
# target 'all'". Build it through cmake --build so lint works under every generator.
add_custom_target(lint
  COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}"
  COMMENT "-Werror build, SDK isolation and clang-tidy")
add_dependencies(lint sdk-isolation)
if(SEL_CLANG_TIDY)
  add_custom_target(lint-tidy
    COMMAND "${SEL_CLANG_TIDY}" -p "${PROJECT_BINARY_DIR}" ${SEL_ALL_SOURCES}
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "clang-tidy over all sources")
else()
  add_custom_target(lint-tidy
    COMMAND "${CMAKE_COMMAND}" -E echo "[INFO] clang-tidy not found. Install with brew install llvm, then reconfigure" VERBATIM
    COMMENT "clang-tidy missing")
endif()
add_dependencies(lint lint-tidy)
