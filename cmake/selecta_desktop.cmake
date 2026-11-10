# Desktop UI dependencies (M3d, DUI-P0-2). SDL3 is found only when the option is on; the option is off by default
# and forced off for the Amiga toolchain, where the UI backends are the RTG and AGA ones (M4).
option(SELECTA_UI_DESKTOP "Build the desktop UI (SDL3 backend, plugins/ui/desktop)" OFF)
if(SEL_AMIGA AND SELECTA_UI_DESKTOP)
  message(STATUS "SELECTA_UI_DESKTOP ignored for the Amiga toolchain")
  set(SELECTA_UI_DESKTOP OFF CACHE BOOL "Build the desktop UI (SDL3 backend, plugins/ui/desktop)" FORCE)
endif()
if(SELECTA_UI_DESKTOP)
  if(EMSCRIPTEN)
    # I694a: the Emscripten SDL3 port (-sUSE_SDL=3, release-3.2.22) installs a sdl3-config.cmake without a version file, so a
    # versioned find_package rejects it.
    find_package(SDL3 REQUIRED CONFIG)
    message(STATUS "SDL3 (Emscripten port) found for the desktop UI")
  else()
    find_package(SDL3 3.2 REQUIRED CONFIG)
    message(STATUS "SDL3 ${SDL3_VERSION} found for the desktop UI")
  endif()
endif()
