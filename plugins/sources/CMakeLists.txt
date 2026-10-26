# Source plugins (kind 7, I517 phase 3): one directory per source, found by glob.
file(GLOB SEL_SOURCE_PLUGIN_DIRS LIST_DIRECTORIES false "${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt")
list(SORT SEL_SOURCE_PLUGIN_DIRS)
foreach(cml IN LISTS SEL_SOURCE_PLUGIN_DIRS)
  get_filename_component(dir "${cml}" DIRECTORY)
  get_filename_component(name "${dir}" NAME)
  add_subdirectory(${name})
endforeach()
