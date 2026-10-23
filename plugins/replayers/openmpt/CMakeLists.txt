# selecta_openmpt: libopenmpt (third_party/libopenmpt, pinned submodule at
# libopenmpt-0.8.4) as a static library for the adapter plugin (row I55).
# C++17 is enabled for this target only; Selecta's own C code keeps
# -Wall -Wextra -Werror -pedantic (top-level CMakeLists.txt applies those to
# C sources only). The vendored sources use upstream's warning set, not ours.
# No external codec dependencies: none of MPT_WITH_ZLIB, MPT_WITH_MPG123,
# MPT_WITH_OGG, MPT_WITH_VORBIS, MPT_WITH_VORBISFILE, MPT_WITH_MINIZ,
# MPT_WITH_MINIMP3, MPT_WITH_STBVORBIS is defined, so compressed MO3 samples
# report unsupported. The system libopenmpt is never used: golden hashes must
# not depend on what a machine has installed.

enable_language(CXX)
include("${PROJECT_SOURCE_DIR}/cmake/openmpt_sources.cmake")

set(OPENMPT_DIR "${PROJECT_SOURCE_DIR}/third_party/libopenmpt")
selecta_openmpt_sources("${OPENMPT_DIR}" SEL_OPENMPT_CXX_SOURCES)

# sample_table.cpp (RP_SAMPLE_GET) reads libopenmpt internals; it is our file, not a vendored one.
add_library(selecta_openmpt STATIC ${SEL_OPENMPT_CXX_SOURCES} sample_table.cpp)
target_include_directories(selecta_openmpt
  PUBLIC "${OPENMPT_DIR}"
  PRIVATE "${OPENMPT_DIR}/src" "${OPENMPT_DIR}/common" "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(selecta_openmpt PRIVATE LIBOPENMPT_BUILD)
target_compile_options(selecta_openmpt PRIVATE -Wall -fvisibility=hidden)
set_target_properties(selecta_openmpt PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# The adapter (C99, Selecta warning set) sees include/selecta plus libopenmpt's
# public header directory, nothing else (T7). sdk-isolation compiles it with
# exactly those two paths.
sel_add_plugin(openmpt 1 adapter.c THIRD_PARTY_INCLUDES "${OPENMPT_DIR}")
target_link_libraries(sel_plugin_openmpt PUBLIC selecta_openmpt)

# The apps link as C (the project language); the C++ runtime that libopenmpt
# needs is added explicitly from the toolchain's implicit C++ link libraries.
target_link_libraries(selecta_openmpt PUBLIC ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
