# Amiga desktop plugin build gate (I494). The cross build cannot run the bake tools it builds, so the desktop plugin
# needs host-built gui_iconbake and gui_fontbake. The canonical Amiga configure (SEL_DESKTOP_AGA off) leaves the
# plugin out on purpose; asking for the AGA desktop without the tools must stop configure instead of producing a
# selecta whose `--ui desktop --backend aga` fails at run time with "desktop backend 'aga' is not mem or sdl".
# Sets SEL_DESKTOP_SKIP to TRUE when the plugin is left out, FATAL_ERROR when it was asked for but cannot be built.
function(sel_desktop_bake_check)
  set(SEL_DESKTOP_SKIP FALSE PARENT_SCOPE)
  if(NOT SEL_AMIGA)
    return()
  endif()
  if(SELECTA_ICONBAKE_EXE AND SELECTA_FONTBAKE_EXE)
    return()
  endif()
  if(SEL_DESKTOP_AGA)
    message(FATAL_ERROR "SEL_DESKTOP_AGA needs the host-built bake tools: pass "
      "-DSELECTA_ICONBAKE_EXE=<build-sdl>/plugins/ui/desktop/gui_iconbake and "
      "-DSELECTA_FONTBAKE_EXE=<build-sdl>/plugins/ui/desktop/gui_fontbake")
  endif()
  message(STATUS "Desktop UI plugin skipped for Amiga: pass -DSELECTA_ICONBAKE_EXE=<host gui_iconbake> and -DSELECTA_FONTBAKE_EXE=<host gui_fontbake> to build it")
  set(SEL_DESKTOP_SKIP TRUE PARENT_SCOPE)
endfunction()
