# Row I1057: a zxart song key plays through the product entry point (selecta-cli script, which owns a library so the remote key goes through the file cache). zxart has no mirror list, so the
# cache fetch must take its download URL from the source's `url` hook (https://zxart.ee/file/id:<id>/filename:<name>).
# The fixture's TLS listener (cert for zxart.ee, CA via SELECTA_CA_FILE) answers that exact path with the PT3 fixture.
# Sentinels: the fixture log holds exactly one GET /file/id:20334/filename:Bomb.pt3 (the hook URL was requested) and the
# played output is not silent.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DCA_FILE=<ca.pem> -DPLUGINS=<module dir>
file(READ "${WORK}/tls_port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "zxart.ee=http://127.0.0.1:${port}")
set(ENV{SELECTA_CA_FILE} "${CA_FILE}")
set(ENV{SELECTA_PLUGIN_PATH} "${PLUGINS}")
unset(ENV{SELECTA_MIRROR})
unset(ENV{SELECTA_MIRRORS_ZXART})
set(cache "${WORK}/i1057-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
set(raw_out "${WORK}/i1057.raw")
set(db "${WORK}/i1057.db")
file(REMOVE "${raw_out}" "${db}")

execute_process(COMMAND "${CLI}" fetch https://zxart.ee/__fixture/reset --method POST OUTPUT_VARIABLE o)
# The script engine owns a library, so the remote key goes through the file cache like in the app.
file(WRITE "${WORK}/i1057.sel"
  "@dump ${raw_out}\nOPEN zxart:20334/Bomb.pt3\nPLAY\n@render 22050\n@sleep 300\n@render 22050\nSTOP\n")
execute_process(COMMAND "${CLI}" script "${WORK}/i1057.sel" --db "${db}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 40)
set(raw "${o}${e}")
if(NOT rc EQUAL 0 OR raw MATCHES "ERROR|open_failed")
  execute_process(COMMAND "${CLI}" fetch https://zxart.ee/__fixture/log --out "${WORK}/i1057-log.json" OUTPUT_VARIABLE lo)
  file(READ "${WORK}/i1057-log.json" flog)
  message(FATAL_ERROR "I1057: fixture log ${flog}\nrender zxart:20334/Bomb.pt3 exited ${rc}\n${raw}")
endif()

execute_process(COMMAND "${CLI}" fetch https://zxart.ee/__fixture/log --out "${WORK}/i1057-log.json" OUTPUT_VARIABLE lo)
file(READ "${WORK}/i1057-log.json" log)
string(JSON n LENGTH "${log}")
set(hits 0)
math(EXPR last "${n} - 1")
foreach(i RANGE ${last})
  string(JSON p GET "${log}" ${i} path)
  if("${p}" STREQUAL "/file/id:20334/filename:Bomb.pt3")
    math(EXPR hits "${hits} + 1")
  endif()
endforeach()
if(NOT hits EQUAL 1)
  message(FATAL_ERROR "I1057: fixture saw ${hits} requests for the url-hook path, want 1\n${log}\n${raw}")
endif()

# Non-silent: the dumped samples hold a nonzero byte.
file(READ "${raw_out}" hex HEX)
string(REGEX REPLACE "00" "" nz "${hex}")
string(LENGTH "${nz}" nzlen)
if(nzlen LESS 2000)
  message(FATAL_ERROR "I1057: output is silent (nonzero hex digits ${nzlen})\n${raw}")
endif()
message(STATUS "zxart_play_ok requests=${hits} nonzero_hex=${nzlen}")
