# Furnace replayer (I430): the headless Furnace engine (tildearrow/furnace, pinned in third_party/furnace/REVISION),
# the engine bbmp uses for its furnace backend. Furnace is GPL-2.0-or-later (see LICENSE), so the module ships as its
# own loadable module under SELECTA_GPL_PLUGINS=ON with the same opt-in as libvgm and AdPlug; the MIT core never links
# it. Built without GUI, SDL, JACK, PortAudio, RtMidi and libsndfile: the engine runs on its dummy audio backend.
enable_language(CXX)
find_package(ZLIB REQUIRED)
set(FURNACE_DIR "${PROJECT_SOURCE_DIR}/third_party/furnace")
include("${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake")

add_library(furnace_engine STATIC EXCLUDE_FROM_ALL ${FURNACE_SOURCES} "${FURNACE_DIR}/extern/fmt/src/format.cc")
target_include_directories(furnace_engine SYSTEM PUBLIC
    "${FURNACE_DIR}/src"
    "${FURNACE_DIR}/src/icon"
    "${FURNACE_DIR}/extern/IconFontCppHeaders"
    "${FURNACE_DIR}/extern/blip_buf"
    "${FURNACE_DIR}/extern/vgsound_emu-modified"
    "${FURNACE_DIR}/extern/fmt/include"
    ${ZLIB_INCLUDE_DIRS})
# Upstream code is not clean under the project's -Wall -Wextra -Werror -pedantic; third-party, so warnings are off.
# The three forced includes are the ones bbmp also needs for current compilers (upstream relies on transitive ones).
target_compile_options(furnace_engine PRIVATE -w -fvisibility=hidden
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include climits>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstdint>"
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include cstring>")
set_target_properties(furnace_engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 14 C_STANDARD 99)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(furnace 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${FURNACE_DIR}/src")
target_include_directories(sel_module_furnace PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sel_module_furnace PRIVATE furnace_engine ${ZLIB_LIBRARIES} ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_furnace PROPERTIES CXX_STANDARD 14)
