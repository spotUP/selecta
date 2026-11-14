# I1378g seg-heap-shared (research thoughts/shared/research/2026-11-14-seg-heap-ownership.md). A LoadSeg module built with
# imports must use the host's C heap: malloc, free and realloc are import thunks (cmake/amiga_seg_imports.txt) and libnix's
# malloc.o, which keeps a private pool the module's fini frees, is not linked. nm on the hunk file: the pool symbols
# (___memorylist, ___initmalloc, ___exitmalloc) must be absent and _malloc/_free/_realloc defined (by the thunks).
# Revert proof: without the list entries and the -u options in sel_add_seg the module links libnix's malloc.o and this fails.
# Inputs: -DNM=<m68k nm> -DSEGS=<;-list of module hunk files>.
foreach(seg IN LISTS SEGS)
  if(NOT EXISTS "${seg}")
    message(FATAL_ERROR "RED[I1378g]: ${seg} missing (build it first)")
  endif()
  execute_process(COMMAND "${NM}" "${seg}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "RED[I1378g]: nm ${seg} failed (rc=${rc})\n${err}")
  endif()
  foreach(sym ___memorylist ___initmalloc ___exitmalloc)
    if(out MATCHES "[0-9a-f]+ [A-Za-z] ${sym}\n")
      message(FATAL_ERROR "RED[I1378g]: ${seg} links libnix's own malloc pool (${sym}): its heap is not the host's")
    endif()
  endforeach()
  foreach(sym _malloc _free _realloc)
    if(NOT out MATCHES "[0-9a-f]+ T ${sym}\n")
      message(FATAL_ERROR "RED[I1378g]: ${seg} has no ${sym} thunk (symbols stripped or the import list lost it)")
    endif()
  endforeach()
  message(STATUS "seg-heap-shared: ${seg} uses the host heap")
endforeach()
