# I1315: the RULES.md "Build (Amiga 68020, Release)" configure line must work on a fresh tree, and the desktop plugin
# (SEL_DESKTOP_AGA defaults to ON, I441) must be part of it with host-compiled bake tools. Before I1315 that configure
# stopped with "SEL_DESKTOP_AGA needs the host-built bake tools". Inputs: -DSRC=<source dir> -DWORK=<scratch tree>.
# Prints "amiga-configure-canonical: skipped" without the bebbo toolchain (the toolchain file's search order).
set(root "$ENV{AMIGA_GCC_ROOT}")
if(NOT root OR NOT EXISTS "${root}/bin/m68k-amigaos-gcc")
  set(root "")
  foreach(cand "$ENV{HOME}/opt/amiga" "/opt/amiga")
    if(NOT root AND EXISTS "${cand}/bin/m68k-amigaos-gcc")
      set(root "${cand}")
    endif()
  endforeach()
endif()
if(NOT root)
  message(STATUS "amiga-configure-canonical: skipped, no m68k-amigaos-gcc")
  return()
endif()
file(REMOVE_RECURSE "${WORK}")
# The exact RULES.md line, build dir aside: no -D for the bake tools, no -DSEL_DESKTOP_AGA.
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${SRC}" -B "${WORK}" -G Ninja -DCMAKE_BUILD_TYPE=Release
    "-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG" "-DCMAKE_TOOLCHAIN_FILE=${SRC}/cmake/amiga-68020.cmake"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "amiga-configure-canonical: configure failed (rc=${rc})\n${err}")
endif()
file(READ "${WORK}/build.ninja" ninja)
foreach(want "gui_iconbake_host" "gui_fontbake_host" "font_baked.c" "gui_icons_baked.c")
  string(FIND "${ninja}" "${want}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "amiga-configure-canonical: build.ninja has no ${want}; the desktop plugin or its host bake is missing")
  endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
message(STATUS "amiga-configure-canonical: all checks passed")
