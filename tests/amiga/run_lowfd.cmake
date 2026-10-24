# Driver: the 68k library opens a database file after descriptor 0 was closed (libnix then hands SQLite fd 0).
# Without SQLITE_MINIMUM_FILE_DESCRIPTOR=0 SQLite closes fd 0 and opens "/dev/null" (the AmigaDOS volume
# "dev:", requester "Please insert volume dev:"), and the open fails. SQLite's unix VFS also seeds its random
# source from "/dev/urandom" (same requester on a real Amiga; under vamos the open just fails), so the run logs
# dos.library opens and fails on any Unix device path.
# Inputs: -DVAMOS -DLOWFD_TEST; WORKING_DIRECTORY must be the repo root (vamos truncates long absolute host paths).
foreach(v VAMOS LOWFD_TEST)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_lowfd.cmake: -D${v} is required")
  endif()
endforeach()

set(db "build/lowfd-test.db")
file(REMOVE "${db}")
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -l dos:info -- "${LOWFD_TEST}" "${db}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
file(REMOVE "${db}")
if(NOT rc EQUAL 0 OR NOT out MATCHES "low_fd_open=ok\n")
  message(FATAL_ERROR "library open on a low descriptor: exit '${rc}'\n${out}${err}")
endif()
string(REGEX MATCHALL "Open: name='/dev/[^']*'" dev_opens "${out}${err}")
if(dev_opens)
  message(FATAL_ERROR "library open touched a Unix device path (requester on AmigaDOS): ${dev_opens}")
endif()
message(STATUS "[OK] library opens a database on a low file descriptor, no Unix device path")
