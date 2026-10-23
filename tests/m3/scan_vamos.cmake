# T71 scan on the 68k binary (I158a): `library scan song:` under vamos must agree with the desktop scan of
# the same directory, through cfg.library, and must probe at least one file (the new code RAN).
# Inputs: MODE=vamos, CLI, VAMOS, AMIGA_CLI, MOD, WORK. Included helpers: tests/m2/lib.cmake.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/../m2/lib.cmake")
get_filename_component(song_dir "${MOD}" DIRECTORY)
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(MODE desktop)
m2_exec(d_out d_rc library scan "${song_dir}")
set(MODE vamos)
m2_exec(v_out v_rc library scan song:)
foreach(f scanned_files skipped_files probed db_songs dirs)
  m2_get("${d_out}" ${f} d_${f})
  m2_get("${v_out}" ${f} v_${f})
endforeach()
if(NOT d_rc EQUAL 0 OR NOT v_rc EQUAL 0)
  message(FATAL_ERROR "RED[T71v]: scan failed desktop rc=${d_rc} vamos rc=${v_rc}\n${d_out}\n${v_out}")
endif()
if(NOT v_probed GREATER 0 OR NOT v_db_songs GREATER 0)
  message(FATAL_ERROR "RED[T71v]: vamos scan probed nothing\n${v_out}")
endif()
# The 68k build carries nuade but not openmpt (plugins list), so it recognises fewer modules than the desktop
# build: files only openmpt probes are skipped there. What must agree: the walk (dirs, files seen =
# scanned + skipped), and the Amiga catalogue is a consistent subset (db_songs == scanned_files <= desktop).
math(EXPR d_seen "${d_scanned_files} + ${d_skipped_files}")
math(EXPR v_seen "${v_scanned_files} + ${v_skipped_files}")
if(NOT d_seen EQUAL v_seen OR NOT d_dirs EQUAL v_dirs)
  message(FATAL_ERROR "RED[T71v]: walk differs desktop seen=${d_seen} dirs=${d_dirs} vamos seen=${v_seen} dirs=${v_dirs}\n${d_out}\n${v_out}")
endif()
if(NOT v_db_songs EQUAL v_scanned_files OR v_scanned_files GREATER d_scanned_files)
  message(FATAL_ERROR "RED[T71v]: vamos catalogue not a consistent subset\n${d_out}\n${v_out}")
endif()
# I158b: the 68k binary opens the DB on the launcher's 4 KiB stack only because main swapped to its own
# stack (sentinel stack_swaps=1 from apps/common/amiga_start.c); no `-s` is given to vamos.
m2_exec(s_out s_rc library stats)
if(NOT s_rc EQUAL 0 OR NOT s_out MATCHES "db_open=1")
  message(FATAL_ERROR "RED[I158b]: library stats under vamos did not open the DB rc=${s_rc}\n${s_out}")
endif()
if(NOT s_out MATCHES "stack_swaps=1 stack_bytes=131072")
  message(FATAL_ERROR "RED[I158b]: the Amiga start-up did not swap to its 128 KiB stack\n${s_out}")
endif()
message("scan_vamos_ok scanned=${v_scanned_files} songs=${v_db_songs}")
