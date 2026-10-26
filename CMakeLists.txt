cmake_minimum_required(VERSION 3.20)
project(selecta VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  # I401: an unspecified host build is Release. The Debug default (no -O) made the live Mac window run the rasterizer unoptimised,
  # about 6 fps at 2560x1600. Debug stays one flag away (-DCMAKE_BUILD_TYPE=Debug, RULES.md) and cross builds keep their old default.
  if(CMAKE_CROSSCOMPILING)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
  else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  endif()
endif()

# Rule 1 of RULES.md: C99, every warning an error, no VLAs or alloca.
# C only: the one C++ target (selecta_openmpt, vendored libopenmpt) must not
# inherit these.
add_compile_options("$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-pedantic;-Wvla;-Wshadow;-Wstrict-prototypes>")

# Tests run host binaries; a cross build cannot run them. T9 drives the
# Amiga binary from the host build directory instead.
if(SEL_AMIGA)
  set(SEL_BUILD_TESTS_DEFAULT OFF)
else()
  set(SEL_BUILD_TESTS_DEFAULT ON)
endif()
option(SEL_BUILD_TESTS "Build and register the ctest suite" ${SEL_BUILD_TESTS_DEFAULT})
option(SEL_TEST_BAD_PLUGIN "Compile a deliberately broken plugin descriptor (T5 negative path)" OFF)

include(cmake/selecta_defs.cmake)
include(cmake/selecta_plugin.cmake)
include(cmake/selecta_lint.cmake)
include(cmake/selecta_desktop.cmake)

add_subdirectory(src)
add_subdirectory(plugins)
include(cmake/selecta_registry.cmake)
add_subdirectory(apps)

if(SEL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
