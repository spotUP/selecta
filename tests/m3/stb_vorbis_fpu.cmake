# I213a: the 68k stb_vorbis objects built for the FPU tier call no libm and no soft-float routine.
# With -m68881 a libm double returns in d0:d1 while FPU code reads fp0, so any floor/ldexp/pow/sin/cos/exp/log
# reference left in the object is a wrong-result bug, and a soft-float helper means -m68881 did not take effect.
# Inputs: -DNM (m68k-amigaos-nm) -DBUILD_DIR (the 68k build tree)
foreach(v NM BUILD_DIR)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "stb_vorbis_fpu.cmake: -D${v} is required")
  endif()
endforeach()

set(forbidden floor ldexp pow sin cos exp log sqrt fabs frexp
  __adddf3 __subdf3 __muldf3 __divdf3 __addsf3 __subsf3 __mulsf3 __divsf3
  __floatsidf __floatsisf __fixdfsi __fixsfsi __extendsfdf2 __truncdfsf2
  __ltdf2 __gtdf2 __ledf2 __gedf2 __nedf2 __eqdf2 __ltsf2 __gtsf2 __lesf2 __gesf2 __nesf2 __eqsf2)

set(objs
  "${BUILD_DIR}/plugins/replayers/stb_vorbis/CMakeFiles/selecta_stb_vorbis.dir/__/__/__/third_party/stb_vorbis/stb_vorbis.c.obj"
  "${BUILD_DIR}/plugins/replayers/stb_vorbis/CMakeFiles/sel_plugin_stb_vorbis.dir/adapter.c.obj")
set(failed "")
foreach(obj IN LISTS objs)
  if(NOT EXISTS "${obj}")
    message(FATAL_ERROR "stb_vorbis_fpu.cmake: ${obj} missing (build build/amiga)")
  endif()
  execute_process(COMMAND "${NM}" -u "${obj}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "nm ${obj}: exit ${rc}\n${err}")
  endif()
  string(REPLACE "\n" ";" lines "${out}")
  set(nsyms 0)
  foreach(line IN LISTS lines)
    if(line MATCHES "^ *U _+([A-Za-z0-9_]+)$")
      math(EXPR nsyms "${nsyms}+1")
      if(CMAKE_MATCH_1 IN_LIST forbidden)
        list(APPEND failed "${obj}: ${CMAKE_MATCH_1}")
      endif()
    endif()
  endforeach()
  if(nsyms EQUAL 0 AND obj MATCHES "stb_vorbis\\.c\\.obj$")
    message(FATAL_ERROR "stb_vorbis_fpu.cmake: nm printed no undefined symbols for ${obj}, not a real object")
  endif()
endforeach()
if(failed)
  string(REPLACE ";" "\n  " msg "${failed}")
  message(FATAL_ERROR "stb_vorbis FPU objects reference libm or soft-float:\n  ${msg}")
endif()
message(STATUS "stb_vorbis_fpu_ok")
