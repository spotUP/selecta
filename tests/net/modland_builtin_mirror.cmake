# I853 modland-builtin-mirror: a synced Modland song plays from the built-in mirror list. Song keys are relative to
# the Modland root (`modland:pub/modules/<format>/<artist>/<file>`, src/library/sources/modland.c), so the built-in
# bases must be Modland roots too; a base ending in pub/modules/ asked for .../pub/modules/pub/modules/... (404) and
# no Modland song opened. No SELECTA_MIRRORS_MODLAND override: the built-in list is what runs, its first HTTP mirror
# mapped to the fixture.
# -DSELECTA=<selecta> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures> -DCAT=<catalogue tsv>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "ftp.amigascne.org=${fx}")
unset(ENV{SELECTA_MIRROR})
unset(ENV{SELECTA_MIRRORS_MODLAND})
unset(ENV{SELECTA_AMIGA_ROUTE})
set(rel "pub/modules/Protracker/test/song.mod")
file(MD5 "${FIXDIR}/ftp.amigascne.org/mirrors/ftp.modland.com/${rel}" want_md5)
set(cache "${WORK}/i853-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
file(WRITE "${WORK}/i853.keys" "q\n")

execute_process(
  COMMAND "${SELECTA}" --output null --size 80x24 --catalogue "${CAT}" --keys "${WORK}/i853.keys"
          --exec "LIBRARY_FETCH modland:${rel}#md5=${want_md5}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I853: selecta exited ${rc}\n${out}\n${err}")
endif()
# fetches=1 is the sentinel that the mirror transport ran; the file must land md5-checked.
if(NOT out MATCHES "(^|\n)fetches=1\n" OR NOT out MATCHES "(^|\n)md5_mismatches=0\n")
  message(FATAL_ERROR "I853: want fetches=1 md5_mismatches=0\n${out}\n${err}")
endif()
if(NOT EXISTS "${cache}/files/modland/${rel}")
  message(FATAL_ERROR "I853: ${rel} missing in the cache (built-in mirror URL wrong?)\n${out}\n${err}")
endif()
file(MD5 "${cache}/files/modland/${rel}" got_md5)
if(NOT got_md5 STREQUAL want_md5)
  message(FATAL_ERROR "I853: cached md5 ${got_md5}, want ${want_md5}")
endif()
