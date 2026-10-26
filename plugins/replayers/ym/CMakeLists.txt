# selecta_stsound: StSound (third_party/stsound, MIT) as a static C++ library for the adapter plugin (row I428).
# C++ is enabled for this target only; Selecta's own C code keeps -Wall -Wextra -Werror -pedantic. The vendored
# sources (patched as listed in third_party/stsound/PATCHES.txt) use upstream's warning set.
enable_language(CXX)
set(STSOUND_DIR "${PROJECT_SOURCE_DIR}/third_party/stsound")
set(SEL_STSOUND_SOURCES
  ${STSOUND_DIR}/digidrum.cpp ${STSOUND_DIR}/Ym2149Ex.cpp ${STSOUND_DIR}/Ymload.cpp
  ${STSOUND_DIR}/YmMusic.cpp ${STSOUND_DIR}/YmUserInterface.cpp ${STSOUND_DIR}/LZH/LzhLib.cpp)

add_library(selecta_stsound STATIC ${SEL_STSOUND_SOURCES})
target_include_directories(selecta_stsound PUBLIC "${STSOUND_DIR}")
target_compile_options(selecta_stsound PRIVATE -w -fvisibility=hidden)
set_target_properties(selecta_stsound PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

# The adapter (C99, Selecta warning set) sees include/selecta and StSound's public C header only (T7).
sel_add_plugin(ym 1 adapter.c THIRD_PARTY_INCLUDES "${STSOUND_DIR}")
target_link_libraries(sel_plugin_ym PUBLIC selecta_stsound)
target_link_libraries(selecta_stsound PUBLIC ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
