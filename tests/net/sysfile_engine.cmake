# I710ga4: engine system-file contract over the net fixture (mirror.sysfile.test serves good.bin / bad.bin).
# -DTEST=<sysfile_engine_test> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "csdb.test=${fx},mirror.sysfile.test=${fx}")
file(SHA256 "${FIXDIR}/mirror.sysfile.test/sysfiles/good.bin" sha)
set(dir "${WORK}/sysfile-engine")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")
execute_process(COMMAND "${TEST}" "${dir}" "${sha}" "${dir}/lib.db" "${FIXDIR}/mirror.sysfile.test/sysfiles/bad.bin"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
message(STATUS "${o}${e}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "sysfile engine: test failed (rc ${rc})")
endif()
