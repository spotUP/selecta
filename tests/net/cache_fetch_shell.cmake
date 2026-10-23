# T75a cache-fetch-shell (row I166a): LIBRARY_FETCH through the `selecta` shell's library handle,
# `selecta --catalogue ... --exec 'LIBRARY_FETCH ...'`. Same fixture mirrors as T75.
# -DSELECTA=<selecta> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures> -DCAT=<catalogue tsv>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "fixture.test=${fx},fast.mirror.test=${fx},bad.mirror.test=${fx}")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODLAND} "http://fast.mirror.test/mods/")
set(rel "Protracker/test/song.mod")
file(MD5 "${FIXDIR}/fast.mirror.test/mods/${rel}" want_md5)
set(cache "${WORK}/t75a-cache")
file(REMOVE_RECURSE "${cache}")
set(ENV{SELECTA_CACHE_DIR} "${cache}")
file(WRITE "${WORK}/t75a.keys" "q\n")

execute_process(
  COMMAND "${SELECTA}" --output null --size 80x24 --catalogue "${CAT}" --keys "${WORK}/t75a.keys"
          --exec "LIBRARY_FETCH modland:${rel}#md5=${want_md5}"
          --exec "LIBRARY_FETCH modland:${rel}#md5=${want_md5}"
          --exec "LIBRARY_FETCH modland:TFMX/test/mdat.song"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "T75a: selecta exited ${rc}\n${out}\n${err}")
endif()
function(expect key want)
  set(got "")
  if(out MATCHES "(^|\n)${key}=([^\n]*)\n")
    set(got "${CMAKE_MATCH_2}")
  endif()
  if(NOT got STREQUAL "${want}")
    message(FATAL_ERROR "T75a ${key}: got '${got}', want ${want}\n${out}\n${err}")
  endif()
endfunction()
# The sentinel lines come from the library handle's cache: the song, its repeat (a hit), the TFMX
# song and its smpl.* companion (3 fetches, 1 companion).
expect(cache_hits 1)
expect(fetches 3)
expect(companions 1)
expect(md5_mismatches 0)
foreach(f "${rel}" "TFMX/test/mdat.song" "TFMX/test/smpl.song")
  if(NOT EXISTS "${cache}/files/modland/${f}")
    message(FATAL_ERROR "T75a: ${f} missing in the cache\n${out}\n${err}")
  endif()
endforeach()
file(MD5 "${cache}/files/modland/${rel}" got_md5)
if(NOT got_md5 STREQUAL want_md5)
  message(FATAL_ERROR "T75a: cached md5 ${got_md5}, want ${want_md5}")
endif()
