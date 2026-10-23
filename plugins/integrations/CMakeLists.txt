# Integration plugins (kind 8; include/selecta/integration.h). Desktop only.
# OS code lives under plugins/integrations/<name>/<os>/ (RULES.md rule 3).

# T38 fake backend: counts what a Now Playing backend would have been given.
sel_add_plugin(nowplaying_test 8 test/nowplaying_test.c)

# macOS Now Playing (MediaPlayer framework). Objective-C is allowed here only.
if(APPLE)
  enable_language(OBJC)
  sel_add_plugin(nowplaying 8 nowplaying/macos/nowplaying.m)
  set_source_files_properties(nowplaying/macos/nowplaying.m PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_compile_options(sel_plugin_nowplaying PRIVATE -fobjc-arc)
  target_compile_options(sel_sdkcheck_nowplaying PRIVATE -fobjc-arc)
  target_link_libraries(sel_plugin_nowplaying PUBLIC "-framework Foundation" "-framework MediaPlayer")
  set(SEL_HAVE_NOWPLAYING 1 CACHE INTERNAL "macOS Now Playing plugin is built")
endif()

# Linux MPRIS over sd-bus, built only when libsystemd is found.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(SEL_SYSTEMD QUIET IMPORTED_TARGET libsystemd)
  endif()
  if(SEL_SYSTEMD_FOUND)
    sel_add_plugin(mpris 8 mpris/linux/mpris.c)
    target_link_libraries(sel_plugin_mpris PUBLIC PkgConfig::SEL_SYSTEMD)
    target_include_directories(sel_sdkcheck_mpris SYSTEM PRIVATE ${SEL_SYSTEMD_INCLUDE_DIRS})
    set(SEL_HAVE_MPRIS 1 CACHE INTERNAL "Linux MPRIS plugin is built")
  else()
    message(STATUS "libsystemd not found: MPRIS integration skipped")
    unset(SEL_HAVE_MPRIS CACHE)
  endif()
endif()
