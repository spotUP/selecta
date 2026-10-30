# Source plugins (kind 7, I517 phase 3): one directory per source, found by glob. CONFIGURE_DEPENDS: a new source
# directory pulled in by a merge reconfigures existing build trees (I725d: build/sdl missed archiveorg).
file(GLOB SEL_SOURCE_PLUGIN_DIRS CONFIGURE_DEPENDS LIST_DIRECTORIES false "${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt")
list(SORT SEL_SOURCE_PLUGIN_DIRS)
foreach(cml IN LISTS SEL_SOURCE_PLUGIN_DIRS)
  get_filename_component(dir "${cml}" DIRECTORY)
  get_filename_component(name "${dir}" NAME)
  add_subdirectory(${name})
endforeach()
