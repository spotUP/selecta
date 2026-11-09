# I1218b / I1075: a desktop dev build carries every player. A build tree configured with SELECTA_GPL_PLUGINS=OFF (or
# SELECTA_KSS_DRIVERS=OFF) builds a Selecta.app without the sid, sndh, zxtune ... replayers, and every song those
# players own fails to open with the "No player for <SOURCE> yet" toast (build/sdl, 2026-11-07 to 2026-11-09). The only
# supported way to leave the GPL players out of a desktop app is the MIT release (SELECTA_RELEASE_MIT_ONLY=ON), so any
# other desktop configure without them stops here instead of building a silently incomplete player set.
function(sel_player_set_check)
  if(NOT SELECTA_UI_DESKTOP OR SEL_AMIGA OR EMSCRIPTEN OR WIN32 OR SELECTA_RELEASE_MIT_ONLY)
    return()
  endif()
  set(missing "")
  if(NOT SELECTA_GPL_PLUGINS)
    list(APPEND missing "SELECTA_GPL_PLUGINS=OFF")
  endif()
  if(NOT SELECTA_KSS_DRIVERS)
    list(APPEND missing "SELECTA_KSS_DRIVERS=OFF")
  endif()
  if(missing)
    list(JOIN missing ", " what)
    message(FATAL_ERROR "Desktop dev build without every player (${what}): sid/sndh/zxtune/kss songs would fail with "
      "\"No player for ... yet\". Reconfigure with -DSELECTA_GPL_PLUGINS=ON -DSELECTA_KSS_DRIVERS=ON, or set "
      "-DSELECTA_RELEASE_MIT_ONLY=ON for the MIT-only release app (I1075, I1218b).")
  endif()
endfunction()
