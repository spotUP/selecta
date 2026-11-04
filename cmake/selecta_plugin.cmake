# sel_add_plugin(<name> <kind> [MODULE] <source>... [THIRD_PARTY_INCLUDES <dir>...])
#
# MODULE (I427a, design 2026-10-30-plugin-loader-sdk D2) builds a loadable library sel_<name>.dylib / .so instead of
# the static library: no SEL_PLUGIN_ENTRY rename (the module exports plain sel_plugin_entry, and nothing else),
# the linker exporting only sel_plugin_entry, not added to SEL_PLUGINS, so neither registry nor core binary ever links it. The loader
# (src/engine/loader_dl.c) finds it on the plugin search path. Output goes to LIBRARY_OUTPUT_DIRECTORY when the
# caller sets the SEL_MODULE_OUTPUT_DIR variable, else the current binary directory.
#
# A plugin is a static library that sees only include/selecta (design doc
# section 12, SDK isolation). Each plugin's entry symbol is renamed to
# sel_plugin_entry_<name> for the static registry; dynamic loaders (M2,
# M5) build the same source without SEL_PLUGIN_ENTRY and get the plain
# sel_plugin_entry export.
#
# THIRD_PARTY_INCLUDES names the include directories of a vendored library an
# adapter plugin wraps (the nuade adapter lists third_party/nuade/include and
# nothing else). They are added to both targets; no other project directory is
# ever visible to a plugin.
#
# plugins/shared is the one other directory a plugin may include from: an internal static helper library
# (selecta_plugin_shared, include root plugins/shared, e.g. "paula/paula_voices.h") that is not public SDK. Plugins that
# need its code link selecta_plugin_shared. A plugin reaching into a sibling plugin directory ("../stream/x.h") breaks
# isolation and fails sel_incheck_<name> (cmake/sel_check_includes.cmake, I228).
#
# Three targets per plugin:
#   sel_plugin_<name>    the library the apps link
#   sel_sdkcheck_<name>  the same sources compiled against include/ only,
#                        collected under the sdk-isolation target (T7)
#   sel_incheck_<name>   resolves every quoted include and rejects any outside the plugin's own directory,
#                        include/selecta, plugins/shared and THIRD_PARTY_INCLUDES (I228)

# I427f (design 2026-10-30-plugin-loader-sdk D6): SELECTA_PLUGIN_LINK picks how every sel_add_plugin links its plugin.
# MODULE (desktop default) builds a loadable library sel_<name>.dylib / .so in ${PROJECT_BINARY_DIR}/modules that the
# runtime loader finds; the core binaries hold engine, registry and loaders only. STATIC (Amiga, web, windows until its
# loader lands, and any build that must link a plugin directly) keeps the generated static registry. The STATIC keyword
# of sel_add_plugin pins one plugin static in either mode (tests that link it directly).
if(SEL_AMIGA OR EMSCRIPTEN OR WIN32)
  set(_sel_link_default STATIC)
else()
  set(_sel_link_default MODULE)
endif()
set(SELECTA_PLUGIN_LINK "${_sel_link_default}" CACHE STRING "How plugins link: MODULE (loadable libraries) or STATIC (registry)")
set_property(CACHE SELECTA_PLUGIN_LINK PROPERTY STRINGS MODULE STATIC)
if(NOT SELECTA_PLUGIN_LINK STREQUAL "MODULE" AND NOT SELECTA_PLUGIN_LINK STREQUAL "STATIC")
  message(FATAL_ERROR "SELECTA_PLUGIN_LINK must be MODULE or STATIC, got '${SELECTA_PLUGIN_LINK}'")
endif()
if(SELECTA_PLUGIN_LINK STREQUAL "MODULE" AND (SEL_AMIGA OR EMSCRIPTEN OR WIN32))
  message(FATAL_ERROR "SELECTA_PLUGIN_LINK=MODULE needs the POSIX loader: Amiga, web and Windows builds stay STATIC")
endif()
if(SELECTA_PLUGIN_LINK STREQUAL "MODULE")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON) # every static library a module absorbs must be position independent
  set(SEL_BUILTIN_MODULE_DIR "${PROJECT_BINARY_DIR}/modules")
endif()

define_property(GLOBAL PROPERTY SEL_BUILTIN_MODULES
  BRIEF_DOCS "Built-in plugin modules (SELECTA_PLUGIN_LINK=MODULE): target names"
  FULL_DOCS "Built-in plugin modules (SELECTA_PLUGIN_LINK=MODULE): target names")
define_property(GLOBAL PROPERTY SEL_PLUGINS
  BRIEF_DOCS "Names of plugins registered with sel_add_plugin"
  FULL_DOCS "Names of plugins registered with sel_add_plugin")

add_custom_target(sdk-isolation
  COMMENT "Every plugin compiles with include/ as its only project include path")

function(sel_add_plugin name kind)
  cmake_parse_arguments(PARSE_ARGV 2 SELP "MODULE;STATIC" "" "THIRD_PARTY_INCLUDES")
  set(sources ${SELP_UNPARSED_ARGUMENTS})
  if(NOT sources)
    message(FATAL_ERROR "sel_add_plugin(${name}): no sources")
  endif()

  set(link_module OFF)
  set(builtin OFF)
  if(SELP_MODULE)
    set(link_module ON)
  elseif(SELECTA_PLUGIN_LINK STREQUAL "MODULE" AND NOT SELP_STATIC)
    set(link_module ON)
    set(builtin ON)
  endif()
  if(link_module)
    if(builtin)
      set(mod sel_plugin_${name}) # same target name as the static form: per-plugin target_link_libraries calls keep working
    else()
      set(mod sel_module_${name})
    endif()
    add_library(${mod} MODULE ${sources})
    target_include_directories(${mod} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_definitions(${mod} PRIVATE SEL_PLUGIN_KIND_BUILD=${kind})
    if(SELP_THIRD_PARTY_INCLUDES)
      target_include_directories(${mod} PRIVATE ${SELP_THIRD_PARTY_INCLUDES})
    endif()
    set_target_properties(${mod} PROPERTIES PREFIX "" OUTPUT_NAME "sel_${name}")
    if(APPLE)
      set_target_properties(${mod} PROPERTIES SUFFIX ".dylib")
      target_link_options(${mod} PRIVATE "-Wl,-exported_symbol,_sel_plugin_entry")
    else()
      set_target_properties(${mod} PROPERTIES SUFFIX ".so")
      target_link_options(${mod} PRIVATE "-Wl,--version-script=${PROJECT_SOURCE_DIR}/cmake/sel_module.map")
    endif()
    if(builtin)
      set_target_properties(${mod} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${SEL_BUILTIN_MODULE_DIR}")
      # A plugin's own directory and plugins/shared are visible to it, as in the static form.
      target_include_directories(${mod} PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared")
      # A module is self-contained: the host-side helpers a plugin calls (sel_tag_find, ...) come from its own copy of
      # selecta_core, the same archive the static form pulls in at the executable's link.
      target_link_libraries(${mod} PRIVATE selecta_core)
      set_property(GLOBAL APPEND PROPERTY SEL_BUILTIN_MODULES ${mod})
    elseif(SEL_MODULE_OUTPUT_DIR)
      set_target_properties(${mod} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${SEL_MODULE_OUTPUT_DIR}")
    endif()
    if(NOT builtin)
      return()
    endif()
  endif()

  if(NOT builtin)
    add_library(sel_plugin_${name} STATIC ${sources})
    target_include_directories(sel_plugin_${name} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_definitions(sel_plugin_${name} PRIVATE
      SEL_PLUGIN_ENTRY=sel_plugin_entry_${name}
      SEL_PLUGIN_KIND_BUILD=${kind})
  endif()

  add_library(sel_sdkcheck_${name} OBJECT EXCLUDE_FROM_ALL ${sources})
  set_target_properties(sel_sdkcheck_${name} PROPERTIES INCLUDE_DIRECTORIES "")
  target_include_directories(sel_sdkcheck_${name} PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(sel_sdkcheck_${name} PRIVATE
    SEL_PLUGIN_ENTRY=sel_plugin_entry_${name}
    SEL_PLUGIN_KIND_BUILD=${kind})
  if(SELP_THIRD_PARTY_INCLUDES)
    target_include_directories(sel_sdkcheck_${name} PRIVATE ${SELP_THIRD_PARTY_INCLUDES})
    if(NOT builtin)
      target_include_directories(sel_plugin_${name} PRIVATE ${SELP_THIRD_PARTY_INCLUDES})
    endif()
  endif()
  target_include_directories(sel_sdkcheck_${name} PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared")
  if(NOT builtin)
    target_include_directories(sel_plugin_${name} PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared")
  endif()
  add_dependencies(sdk-isolation sel_sdkcheck_${name})

  # Own directory: plugins/<kind>/<name> for a plugin under plugins/, else the first source's directory.
  set(own_dir "")
  foreach(src IN LISTS sources)
    cmake_path(ABSOLUTE_PATH src BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE abs_src)
    if(abs_src MATCHES "^${PROJECT_SOURCE_DIR}/")
      if(abs_src MATCHES "^(${PROJECT_SOURCE_DIR}/plugins/[^/]+/[^/]+)/")
        set(own_dir "${CMAKE_MATCH_1}")
      else()
        cmake_path(GET abs_src PARENT_PATH own_dir)
      endif()
      break()
    endif()
  endforeach()
  # A plugin may include headers from its own directory (rule 3). Sources generated in the binary directory
  # (desktop_ui font_baked.c) need it on the include path; both targets get it so the SDK check builds them too.
  if(own_dir)
    foreach(t sel_plugin_${name} sel_sdkcheck_${name})
      target_include_directories(${t} PRIVATE "${own_dir}")
    endforeach()
  endif()
  set(abs_sources "")
  foreach(src IN LISTS sources)
    cmake_path(ABSOLUTE_PATH src BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE abs_src)
    list(APPEND abs_sources "${abs_src}")
  endforeach()
  list(JOIN abs_sources "|" sources_arg)
  list(JOIN SELP_THIRD_PARTY_INCLUDES "|" tp_arg)
  add_custom_target(sel_incheck_${name}
    COMMAND "${CMAKE_COMMAND}" -DPLUGIN=${name} "-DOWN_DIR=${own_dir}" "-DSOURCES=${sources_arg}"
      "-DTHIRD_PARTY=${tp_arg}" "-DSDK_DIR=${PROJECT_SOURCE_DIR}/include/selecta"
      "-DSHARED_DIR=${PROJECT_SOURCE_DIR}/plugins/shared"
      -P "${PROJECT_SOURCE_DIR}/cmake/sel_check_includes.cmake"
    VERBATIM COMMENT "Include isolation of plugin ${name}")
  add_dependencies(sdk-isolation sel_incheck_${name})

  if(NOT builtin)
    set_property(GLOBAL APPEND PROPERTY SEL_PLUGINS ${name})
  endif()
endfunction()
