# I1218 hvsc-builtin-mirror: an hvsc song key hvsc:<path> is hosted by Modland (SEL_SRC_HOSTED_BY), so it fetches from
# the built-in Modland mirror list under pub/modules/HVSC/<path>. hvsc had its own base https://hvsc.de/download/
# (404 for bare MUSICIANS/... paths): Johan's 404 on the Chris Huelsbeck SIDs. No SELECTA_MIRRORS override.
# -DSELECTA=<selecta> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures> -DCAT=<catalogue tsv>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "ftp.amigascne.org=${fx}")
unset(ENV{SELECTA_MIRROR})
unset(ENV{SELECTA_MIRRORS_MODLAND})
unset(ENV{SELECTA_MIRRORS_HVSC})
unset(ENV{SELECTA_AMIGA_ROUTE})
set(hostrel "pub/modules/HVSC/MUSICIANS/H/Huelsbeck_Chris/Test_Song.sid")
set(rel "MUSICIANS/H/Huelsbeck_Chris/Test_Song.sid")
file(MD5 "${FIXDIR}/ftp.amigascne.org/mirrors/ftp.modland.com/${hostrel}" want_md5)
set(cache "${WORK}/i1218-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
file(WRITE "${WORK}/i1218.keys" "q\n")

execute_process(
  COMMAND "${SELECTA}" --output null --size 80x24 --catalogue "${CAT}" --keys "${WORK}/i1218.keys"
          --exec "LIBRARY_FETCH hvsc:${rel}#md5=${want_md5}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1218: selecta exited ${rc}\n${out}\n${err}")
endif()
# fetches=1 is the sentinel that the mirror transport ran; the file must land md5-checked.
if(NOT out MATCHES "(^|\n)fetches=1\n" OR NOT out MATCHES "(^|\n)md5_mismatches=0\n")
  message(FATAL_ERROR "I1218: want fetches=1 md5_mismatches=0\n${out}\n${err}")
endif()
if(NOT EXISTS "${cache}/files/hvsc/${rel}")
  message(FATAL_ERROR "I1218: ${rel} missing in the cache (built-in mirror URL wrong?)\n${out}\n${err}")
endif()
file(MD5 "${cache}/files/hvsc/${rel}" got_md5)
if(NOT got_md5 STREQUAL want_md5)
  message(FATAL_ERROR "I1218: cached md5 ${got_md5}, want ${want_md5}")
endif()
