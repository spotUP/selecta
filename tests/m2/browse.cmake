# T57 amiga-library-browse: `selecta-cli library browse <dir>` under vamos lists 3 files + 1 subdirectory.
# Sentinel dir_entries=; the fixture dir is mounted as volume songs: (vamos -V). LIBRARY_BROWSE must not
# answer browse_failed. Inputs: MODE=vamos VAMOS AMIGA_CLI CLI WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/songs/sub")
foreach(f a.mod b.mod c.mod)
  file(WRITE "${WORK}/songs/${f}" "x")
endforeach()
set(VAMOS_VOLUMES -V songs:${WORK}/songs)
m2_exec(out rc library browse songs:)
if(out MATCHES "browse_failed")
  message(FATAL_ERROR "RED[T57]: LIBRARY_BROWSE answers browse_failed on 68k (dir.c Examine/ExNext missing).\n${out}")
endif()
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "RED[T57]: 'library browse' exited ${rc} (command not implemented).\n${out}")
endif()
m2_check(T57 "${out}" "dir_entries==4")
message(STATUS "[OK] T57 ${MODE}")
