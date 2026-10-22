# Source list of libopenmpt, read from upstream's root Makefile at configure
# time (third_party/libopenmpt, pinned submodule) instead of copied, so a
# submodule bump cannot leave a stale list (RULES.md rule 5: adapt, never fork).
#
# Collects the `$(sort $(wildcard dir/*.cpp))` entries of the blocks
# COMMON_CXX_SOURCES, SOUNDLIB_CXX_SOURCES and LIBOPENMPT_CXX_SOURCES, which
# together are the C API library (libopenmpt/libopenmpt_c.cpp included). The
# SOUNDLIB_TEST block (test-only writers) and the HACK_ARCHIVE_SUPPORT block
# (unarchiver, needs zlib) are left out on purpose.
#
# selecta_openmpt_sources(<openmpt dir> <out var>) sets <out var> to absolute paths.

function(selecta_openmpt_sources OPENMPT_DIR OUT_VAR)
  set(_makefile "${OPENMPT_DIR}/Makefile")
  if(NOT EXISTS "${_makefile}")
    message(FATAL_ERROR "libopenmpt submodule missing: run `git submodule update --init third_party/libopenmpt`")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_makefile}")
  # Whole-file regex, not file(STRINGS): Makefile lines carry ; and [ ] which
  # break CMake list iteration. A block is the assignment line plus the
  # indented continuation lines that follow it.
  file(READ "${_makefile}" _content)
  string(REGEX MATCHALL "(COMMON|SOUNDLIB|LIBOPENMPT)_CXX_SOURCES \\+= \\\\\n( [^\n]*\n)+" _blocks "${_content}")
  set(_sources "")
  foreach(_block IN LISTS _blocks)
    string(REGEX MATCHALL "wildcard [^ )]+\\.cpp" _wildcards "${_block}")
    foreach(_w IN LISTS _wildcards)
      string(REPLACE "wildcard " "" _pattern "${_w}")
      file(GLOB _found "${OPENMPT_DIR}/${_pattern}")
      list(SORT _found)
      list(APPEND _sources ${_found})
    endforeach()
  endforeach()
  if(NOT _sources)
    message(FATAL_ERROR "libopenmpt: no sources found in ${_makefile}; the Makefile layout changed")
  endif()
  set(${OUT_VAR} "${_sources}" PARENT_SCOPE)
endfunction()
