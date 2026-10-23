# T69 mirrors-ranking (row I156): `selecta-cli library fetch modland:<path>` over three fixture
# mirrors (dead, 4 s first byte, fast), the home mirror, and the Amiga route.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DFIXDIR=<tests/net/fixtures>
# dead.mirror.test maps to a closed loopback port, so its connect is refused.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP}
  "fixture.test=${fx},fast.mirror.test=${fx},slow.mirror.test=${fx},home.mirror.test=${fx},devilbox.uprough.net=${fx},dead.mirror.test=http://127.0.0.1:1")
set(mods "/mods/")
set(list "http://dead.mirror.test${mods},http://slow.mirror.test${mods},http://fast.mirror.test${mods}")
set(rel "Protracker/test/song.mod")
file(SHA256 "${FIXDIR}/fast.mirror.test/mods/${rel}" want_sha)
file(SIZE "${FIXDIR}/fast.mirror.test/mods/${rel}" want_size)
set(out_dir "${WORK}/t69")
file(MAKE_DIRECTORY "${out_dir}")

function(expect what got want)
  if(NOT "${got}" STREQUAL "${want}")
    message(FATAL_ERROR "T69 ${what}: got '${got}', want '${want}'\n${last_raw}")
  endif()
endfunction()

# fetch(<prefix> <args...>): runs `library fetch`, sets <prefix>_<counter> and last_raw.
function(mfetch prefix)
  file(REMOVE "${out_dir}/${prefix}.bin")
  execute_process(COMMAND "${CLI}" library fetch ${ARGN} --out "${out_dir}/${prefix}.bin"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  set(last_raw "${o}${e}" PARENT_SCOPE)
  set(${prefix}_rc "${rc}" PARENT_SCOPE)
  foreach(k mirror_tries mirror_retries mirror_served mirror_home mirror_order body_bytes mirror_url
      mirror_round_1_tries mirror_round_1_retries mirror_round_1_served
      mirror_round_2_tries mirror_round_2_retries mirror_round_2_served
      mirror_0_failures mirror_1_failures mirror_2_failures mirror_2_ttfb_ms)
    if(o MATCHES "(^|\n)${k}=([^\n]*)")
      set(${prefix}_${k} "${CMAKE_MATCH_2}" PARENT_SCOPE)
    else()
      set(${prefix}_${k} "" PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

# hits(<var> <host> <path>): requests the fixture server logged for host + path.
function(hits var host path)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${out_dir}/log.json"
    OUTPUT_VARIABLE o RESULT_VARIABLE rc)
  file(READ "${out_dir}/log.json" logjson)
  string(JSON n LENGTH "${logjson}")
  set(count 0)
  if(n GREATER 0)
    math(EXPR last "${n} - 1")
    foreach(i RANGE ${last})
      string(JSON h GET "${logjson}" ${i} host)
      string(JSON p GET "${logjson}" ${i} path)
      if(h STREQUAL "${host}" AND p STREQUAL "${path}")
        math(EXPR count "${count} + 1")
      endif()
    endforeach()
  endif()
  set(${var} "${count}" PARENT_SCOPE)
endfunction()

function(reset_log)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST
    OUTPUT_VARIABLE o RESULT_VARIABLE rc)
endfunction()

# Ranking: round 1 runs the list in order. The dead mirror fails (1 retry) and the 4 s mirror
# trips the 3 s first-byte limit (1 retry), the fast mirror serves. After that one measurement
# round the fast mirror ranks first: round 2 makes 1 try and 0 retries, and the dead mirror
# has been attempted exactly once in total.
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODLAND} "${list}")
reset_log()
mfetch(rank modland:${rel} --rounds 2)
expect("exit code" "${rank_rc}" 0)
expect("round 1 tries" "${rank_mirror_round_1_tries}" 3)
expect("round 1 retries" "${rank_mirror_round_1_retries}" 2)
expect("round 1 served by the fast mirror" "${rank_mirror_round_1_served}" 2)
expect("round 2 tries" "${rank_mirror_round_2_tries}" 1)
expect("round 2 retries" "${rank_mirror_round_2_retries}" 0)
expect("round 2 served by the fast mirror" "${rank_mirror_round_2_served}" 2)
expect("rank order after the round" "${rank_mirror_order}" "2,0,1")
expect("dead mirror attempted once" "${rank_mirror_0_failures}" 1)
expect("fast mirror measured" "${rank_mirror_2_failures}" 0)
if(rank_mirror_2_ttfb_ms STREQUAL "" OR rank_mirror_2_ttfb_ms LESS 1)
  message(FATAL_ERROR "T69 fast mirror has no first-byte measurement: ${last_raw}")
endif()
expect("body bytes" "${rank_body_bytes}" "${want_size}")
file(SHA256 "${out_dir}/rank.bin" got_sha)
expect("body sha256" "${got_sha}" "${want_sha}")
hits(fast_hits fast.mirror.test "/mods/${rel}")
expect("server saw the fast mirror twice" "${fast_hits}" 2)
hits(dead_hits dead.mirror.test "/mods/${rel}")
expect("server never saw the dead mirror" "${dead_hits}" 0)

# Home mirror first: SELECTA_MIRROR leads the list of the source, so one try serves.
set(ENV{SELECTA_MIRROR} "http://home.mirror.test${mods}")
reset_log()
mfetch(home modland:${rel})
expect("home exit code" "${home_rc}" 0)
expect("home flag" "${home_mirror_home}" 1)
expect("home tries" "${home_mirror_tries}" 1)
expect("home served first" "${home_mirror_served}" 0)
expect("home order" "${home_mirror_order}" "0,1,2,3")
hits(home_hits home.mirror.test "/mods/${rel}")
expect("server saw the home mirror" "${home_hits}" 1)
hits(home_other fast.mirror.test "/mods/${rel}")
expect("list not touched when home serves" "${home_other}" 0)

# A dead home mirror costs one retry, then the list serves.
set(ENV{SELECTA_MIRROR} "http://dead.mirror.test${mods}")
set(ENV{SELECTA_MIRRORS_MODLAND} "http://fast.mirror.test${mods}")
mfetch(deadhome modland:${rel})
expect("dead home exit code" "${deadhome_rc}" 0)
expect("dead home tries" "${deadhome_mirror_tries}" 2)
expect("dead home retries" "${deadhome_mirror_retries}" 1)
expect("dead home served by the list" "${deadhome_mirror_served}" 1)

# Amiga route: with an Amiga machine the request is /api/mirror/modland/<path> on the DEViLBOX
# endpoint, plain HTTP; the source's list is not used.
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODLAND} "${list}")
set(ENV{SELECTA_DEVILBOX_URL} "http://devilbox.uprough.net")
reset_log()
mfetch(amiga modland:${rel} --machine a1200)
expect("amiga exit code" "${amiga_rc}" 0)
expect("amiga tries" "${amiga_mirror_tries}" 1)
expect("amiga url" "${amiga_mirror_url}" "http://devilbox.uprough.net/api/mirror/modland/${rel}")
hits(amiga_hits devilbox.uprough.net "/api/mirror/modland/${rel}")
expect("server saw the DEViLBOX mirror route" "${amiga_hits}" 1)
hits(amiga_list fast.mirror.test "/mods/${rel}")
expect("amiga ignores the list" "${amiga_list}" 0)
file(SHA256 "${out_dir}/amiga.bin" got_sha)
expect("amiga body sha256" "${got_sha}" "${want_sha}")
