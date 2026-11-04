# AdPlug replayer (I696): AdPlug (Simon Peter and contributors, pinned in third_party/adplug/REVISION) with libbinio and
# the Nuked OPL3 core, the engine bbmp uses for OPL/AdLib songs. AdPlug is LGPL-2.1-or-later (one file dual GPL/LGPL,
# taken under the LGPL; the aPLib depacker is freeware): the module links it statically, so it ships as its own
# loadable module under SELECTA_GPL_PLUGINS=ON with the same opt-in as libvgm; the MIT core never links it. See LICENSE.
enable_language(CXX)
set(ADPLUG_DIR "${PROJECT_SOURCE_DIR}/third_party/adplug/src")
set(BINIO_DIR "${PROJECT_SOURCE_DIR}/third_party/libbinio/src")

file(GLOB ADPLUG_SOURCES CONFIGURE_DEPENDS "${ADPLUG_DIR}/*.cpp" "${ADPLUG_DIR}/*.c")
file(GLOB BINIO_SOURCES CONFIGURE_DEPENDS "${BINIO_DIR}/*.cpp")
# Only the Nuked OPL3 emulator and the player sources are vendored (no other emulator cores, no hardware outputs).
add_library(adplug_engine STATIC EXCLUDE_FROM_ALL ${ADPLUG_SOURCES} ${BINIO_SOURCES})
target_include_directories(adplug_engine PUBLIC "${ADPLUG_DIR}" "${BINIO_DIR}")
# Upstream code is not clean under the project's -Wall -Wextra -Werror -pedantic; third-party, so warnings are off.
target_compile_options(adplug_engine PRIVATE -w -fvisibility=hidden)
target_compile_definitions(adplug_engine PRIVATE stricmp=strcasecmp)
set_target_properties(adplug_engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 14)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(adplug 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${ADPLUG_DIR}" "${BINIO_DIR}")
target_include_directories(sel_module_adplug PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sel_module_adplug PRIVATE adplug_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_adplug PROPERTIES CXX_STANDARD 14)
