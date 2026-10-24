# I218d driver: a Latin-1 file name crosses the AmigaDOS boundary both ways under vamos, through
# `selecta-cli library browse <dir> --names` (library browse -> sel_dir_each -> Lock/Examine/ExNext, then fopen).
# vamos hands the 68k binary host names as Latin-1 bytes (0xF6 for o-umlaut) and maps Latin-1 paths back, which is
# what a real Amiga volume holds. macOS refuses to create a Latin-1 named host file (APFS: illegal byte sequence),
# so the fixture is the UTF-8 host name; the 68k side sees the Latin-1 spelling either way.
# Expect: entry=<name> arrives as UTF-8 and open=<name> ok (the UTF-8 name converts back to Latin-1 before DOS).
# Inputs: -DVAMOS -DAMIGA_CLI; WORKING_DIRECTORY must be the repo root (vamos truncates long absolute host paths).
foreach(v VAMOS AMIGA_CLI)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "run_latin1_name.cmake: -D${v} is required")
  endif()
endforeach()

set(dir "build/latin1-name-test")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
file(WRITE "${dir}/Björk Ärger.mod" "x")
file(WRITE "${dir}/plain.mod" "x")

execute_process(COMMAND "${VAMOS}" -C 68020 -m 8192 -- "${AMIGA_CLI}" library browse "${dir}" --names
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 60)
file(REMOVE_RECURSE "${dir}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "library browse --names: exit '${rc}'\n${out}${err}")
endif()
foreach(want "entry=Björk Ärger.mod\n" "open=Björk Ärger.mod ok\n" "open=plain.mod ok\n")
  string(FIND "${out}" "${want}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "missing '${want}' in:\n${out}${err}")
  endif()
endforeach()
message(STATUS "[OK] Latin-1 name listed as UTF-8 and opened by that name")
