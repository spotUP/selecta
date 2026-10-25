# dui-m68k-nofloat (DUI-P6-3, plan section 1 item 6): src/gui and src/uimodel build for m68k-amigaos -m68020 with the
# Amiga toolchain, and the archives reference no soft-float helper. -DSRC -DWORK -DGEN -DCMAKE -DNM
# 1. configure and build selecta_gui and selecta_uimodel with cmake/amiga-68020.cmake (tests/dui/m68k/CMakeLists.txt);
# 2. nm of both archives lists the symbol set; the soft-float helpers of libgcc (single and double: arithmetic,
#    compare, convert) and the libm calls a float expression pulls in must be absent as undefined symbols.
#    Instrument check: the list must define a known symbol of each library, or an empty nm would pass for nothing.
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND "${CMAKE}" -S "${SRC}/tests/dui/m68k" -B "${WORK}" -G "${GEN}"
  "-DCMAKE_TOOLCHAIN_FILE=${SRC}/cmake/amiga-68020.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "m68k configure failed:\n${out}\n${err}")
endif()
execute_process(COMMAND "${CMAKE}" --build "${WORK}" --target selecta_gui selecta_uimodel
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "m68k build failed:\n${out}\n${err}")
endif()
file(GLOB_RECURSE libs "${WORK}/*.a")
list(LENGTH libs nlibs)
if(NOT nlibs EQUAL 2)
  message(FATAL_ERROR "expected 2 archives (selecta_gui, selecta_uimodel), found ${nlibs}: ${libs}")
endif()
execute_process(COMMAND "${NM}" ${libs} OUTPUT_VARIABLE syms ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "nm failed: ${err}")
endif()
foreach(known sel_gui_canvas_init sel_uim_init)
  if(NOT syms MATCHES "[ \t]T _?${known}\n")
    message(FATAL_ERROR "instrument check: ${known} is not defined in the nm output; the symbol check would pass for nothing")
  endif()
endforeach()
set(soft
  __addsf3 __subsf3 __mulsf3 __divsf3 __negsf2 __adddf3 __subdf3 __muldf3 __divdf3 __negdf2
  __eqsf2 __nesf2 __ltsf2 __lesf2 __gtsf2 __gesf2 __cmpsf2 __unordsf2
  __eqdf2 __nedf2 __ltdf2 __ledf2 __gtdf2 __gedf2 __cmpdf2 __unorddf2
  __floatsisf __floatsidf __floatunsisf __floatunsidf __floatdisf __floatdidf
  __fixsfsi __fixdfsi __fixunssfsi __fixunsdfsi __fixsfdi __fixdfdi
  __extendsfdf2 __truncdfsf2 __powidf2 __powisf2
  sqrt sqrtf floor ceil pow powf fabs fmod)
set(found "")
foreach(s IN LISTS soft)
  if(syms MATCHES "[ \t]U _?${s}\n")
    list(APPEND found ${s})
  endif()
endforeach()
if(found)
  message(FATAL_ERROR "floating-point helpers referenced by src/gui or src/uimodel: ${found}")
endif()
message(STATUS "dui-m68k-nofloat ok: ${nlibs} archives, none of the soft-float helpers referenced")
