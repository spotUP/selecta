# Driver (I225): a new library database is created, grown, shrunk with VACUUM and reopened through the 68k
# library under vamos. The program (tests/amiga/dbcreate_test.c) fails on any seek past the end of a file and
# returns the libnix ftruncate result, so SQLite's lseek fallback and an unwrapped ftruncate both fail here.
# Inputs: -DVAMOS -DDBCREATE_TEST; WORKING_DIRECTORY must be the repo root (vamos truncates long absolute host paths).
foreach(v VAMOS DBCREATE_TEST)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_dbcreate.cmake: -D${v} is required")
  endif()
endforeach()

set(db "build/dbcreate-test.db")
file(REMOVE "${db}" "${db}-journal")
execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -- "${DBCREATE_TEST}" "${db}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 90)
file(REMOVE "${db}" "${db}-journal")
if(NOT rc EQUAL 0 OR NOT out MATCHES "db_create=ok\n")
  message(FATAL_ERROR "new library database on libnix: exit '${rc}'\n${out}${err}")
endif()
message(STATUS "[OK] new library database created, shrunk and reopened with no seek past the end: ${out}")
