# DUI-P6-3: src/gui and src/uimodel alone, cross-built for m68k-amigaos -m68020 (driven by ../m68k_nofloat.cmake).
# A stand-alone project so the check needs neither the vendored players nor the rest of the tree. selecta_core is
# only a header provider here: the two libraries must not need a symbol from it.
cmake_minimum_required(VERSION 3.20)
project(dui_m68k C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options("$<$<COMPILE_LANGUAGE:C>:-Wall;-Wextra;-Werror;-pedantic;-Wvla;-Wshadow;-Wstrict-prototypes>")
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(PROJECT_SOURCE_DIR "${ROOT}")
add_library(selecta_core INTERFACE)
target_include_directories(selecta_core INTERFACE "${ROOT}/include")
add_subdirectory("${ROOT}/src/gui" gui)
add_subdirectory("${ROOT}/src/uimodel" uimodel)
