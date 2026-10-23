# T18: rule 4 (no floating point in the engine) checked from the 68k object
# files, not from the source. Runs m68k-amigaos-nm over the objects of the
# engine (src/engine), the protocol parser, the mixer, the library, the wav output and the null output, and
# fails when any of them references a soft-float runtime routine.
# Inputs: -DNM (m68k-amigaos-nm) -DBUILD_DIR (the 68k build tree)
foreach(v NM BUILD_DIR)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "no_float.cmake: -D${v} is required")
  endif()
endforeach()

set(forbidden __adddf3 __subdf3 __muldf3 __divdf3 __addsf3 __subsf3 __mulsf3 __divsf3
  __floatsidf __floatsisf __fixdfsi __fixsfsi __extendsfdf2 __truncdfsf2)

file(GLOB_RECURSE objs
  "${BUILD_DIR}/src/CMakeFiles/selecta_core.dir/engine/*.obj"
  "${BUILD_DIR}/src/CMakeFiles/selecta_core.dir/protocol/*.obj"
  "${BUILD_DIR}/src/CMakeFiles/selecta_mixer.dir/mixer/*.obj"
  "${BUILD_DIR}/plugins/CMakeFiles/sel_plugin_wav.dir/*.obj"
  "${BUILD_DIR}/plugins/CMakeFiles/sel_plugin_null.dir/*.obj"
  "${BUILD_DIR}/src/library/CMakeFiles/selecta_library.dir/*.obj")
list(LENGTH objs nobjs)
# engine (engine, format, prefs, queue, view, voices, spsc, scheduler) + protocol (parse)
# + mixer (mixer, resample) + wav + null
# + library (local, dir, playlist, hippo, m3u, pls, xspf)
if(NOT nobjs EQUAL 20)
  message(FATAL_ERROR "expected 20 objects (8 engine, 1 protocol, 2 mixer, wav, null, 7 library), found ${nobjs}:\n${objs}")
endif()

set(bad "")
foreach(obj IN LISTS objs)
  execute_process(COMMAND "${NM}" -u "${obj}"
    OUTPUT_VARIABLE undef ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "nm ${obj}: exit ${rc}\n${err}")
  endif()
  foreach(sym IN LISTS forbidden)
    # The 68k nm prints C symbols with a leading underscore (___muldf3).
    if(undef MATCHES "(^|\n)[ ]*U _?${sym}(\n|$)")
      string(APPEND bad "  ${obj}: ${sym}\n")
    endif()
  endforeach()
endforeach()
if(bad)
  message(FATAL_ERROR "floating point in the engine (rule 4):\n${bad}")
endif()
message(STATUS "[OK] no soft-float references in ${nobjs} engine, mixer, wav, null and library objects")
