# T90 fetch part (row I188): the gain of a song comes from the DEViLBOX loudness endpoint on an Amiga machine
# (no analysis), and a desktop analysis is submitted with one PUT carrying the bearer token.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DSONGS=<tests/m3/genre>
# ambient.mod (md5 b0ecddaa...): GET 200, lufs -23.4, so gain +5.40 dB (540 centi-dB) from the -18 LUFS target.
# techno.mod  (md5 67baee51...): GET 404 unknown hash, PUT 200.
# nogenre.mod (md5 3c6d327f...): no route, text/plain 404 = endpoint missing.
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=${fx},fixture.test=${fx}")
set(ENV{SELECTA_DEVILBOX_URL} "http://devilbox.uprough.net")

set(cfg "${WORK}/t90-config")
file(REMOVE_RECURSE "${cfg}")
file(MAKE_DIRECTORY "${cfg}")
set(empty_cfg "${WORK}/t90-config-empty")
file(REMOVE_RECURSE "${empty_cfg}")
file(MAKE_DIRECTORY "${empty_cfg}")
# The token comes from the I173 login (single token store), not from a hand-written file.
file(WRITE "${WORK}/t90-pw" "fixture-password\n")
execute_process(COMMAND "${CLI}" devilbox login loudness --password-stdin --config-dir "${cfg}" INPUT_FILE "${WORK}/t90-pw"
  OUTPUT_VARIABLE lo ERROR_VARIABLE le RESULT_VARIABLE lrc)
file(REMOVE "${WORK}/t90-pw")
if(NOT lrc EQUAL 0 OR NOT EXISTS "${cfg}/devilbox.token")
  message(FATAL_ERROR "T90 fetch: devilbox login failed rc=${lrc}\n${lo}${le}")
endif()

function(reset_log)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/reset --method POST OUTPUT_VARIABLE o)
endfunction()

function(run_loudness out db)
  file(REMOVE "${db}" "${db}-wal" "${db}-shm")
  execute_process(COMMAND "${CLI}" loudness --db "${db}" ${ARGN} OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "T90 fetch: selecta-cli loudness ${ARGN} failed rc=${rc}\n${o}${e}")
  endif()
  set(${out} "${o}${e}" PARENT_SCOPE)
endfunction()

# Reads the fixture log; sets <var>_get and <var>_put (requests on /api/loudness/) and <var>_json.
function(loudness_log var)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/t90-log.json" OUTPUT_VARIABLE o)
  file(READ "${WORK}/t90-log.json" logjson)
  string(JSON n LENGTH "${logjson}")
  set(g 0)
  set(p 0)
  if(n GREATER 0)
    math(EXPR last "${n} - 1")
    foreach(i RANGE ${last})
      string(JSON path GET "${logjson}" ${i} path)
      string(JSON method GET "${logjson}" ${i} method)
      if(path MATCHES "^/api/loudness/")
        if(method STREQUAL "GET")
          math(EXPR g "${g} + 1")
        elseif(method STREQUAL "PUT")
          math(EXPR p "${p} + 1")
        endif()
      endif()
    endforeach()
  endif()
  set(${var}_get ${g} PARENT_SCOPE)
  set(${var}_put ${p} PARENT_SCOPE)
  set(${var}_json "${logjson}" PARENT_SCOPE)
endfunction()

function(expect_match text pattern what)
  if(NOT text MATCHES "${pattern}")
    message(FATAL_ERROR "T90 fetch: ${what}: want /${pattern}/\n${text}")
  endif()
endfunction()

function(expect_eq got want what)
  if(NOT got EQUAL want)
    message(FATAL_ERROR "T90 fetch: ${what}=${got}, want ${want}")
  endif()
endfunction()

# ---- 1. Amiga machine: the gain comes from the endpoint, no analysis, no PUT
reset_log()
run_loudness(r1 "${WORK}/t90-1.db" --machine a1200 --config-dir "${cfg}" "${SONGS}/ambient.mod")
expect_match("${r1}" "loudness_source=devilbox gain_cdb=540 hash=b0ecddaa870b6f7a22733786136fb5b0" "fetch on a1200")
expect_match("${r1}" "loudness_put=none" "no PUT after a fetch")
loudness_log(l1)
expect_eq(${l1_get} 1 "ambient GET requests")
expect_eq(${l1_put} 0 "ambient PUT requests")

# ---- 2. Amiga machine, unknown hash: no analysis runs, nothing stored, no PUT
reset_log()
run_loudness(r2 "${WORK}/t90-2.db" --machine a1200 --config-dir "${cfg}" "${SONGS}/techno.mod")
expect_match("${r2}" "loudness_source=none" "unknown hash on a1200 stays unanalysed")
expect_match("${r2}" "loudness_put=none" "a1200 never submits")
loudness_log(l2)
expect_eq(${l2_put} 0 "a1200 PUT requests")

# ---- 3. Desktop, token present: unknown hash, local analysis, one PUT with the bearer token
reset_log()
run_loudness(r3 "${WORK}/t90-3.db" --config-dir "${cfg}" "${SONGS}/techno.mod")
expect_match("${r3}" "loudness_source=analysis" "desktop analysis")
expect_match("${r3}" "loudness_get=1 loudness_put=ok" "one GET then PUT ok")
loudness_log(l3)
expect_eq(${l3_get} 1 "techno GET requests")
expect_eq(${l3_put} 1 "techno PUT requests")
string(JSON n3 LENGTH "${l3_json}")
math(EXPR last3 "${n3} - 1")
set(saw_put 0)
foreach(i RANGE ${last3})
  string(JSON method GET "${l3_json}" ${i} method)
  if(method STREQUAL "PUT")
    set(saw_put 1)
    string(JSON auth GET "${l3_json}" ${i} headers authorization)
    string(JSON body GET "${l3_json}" ${i} body)
    string(JSON ppath GET "${l3_json}" ${i} path)
    expect_match("${auth}" "^Bearer fixture-jwt-loudness$" "PUT authorization")
    expect_match("${ppath}" "^/api/loudness/67baee51cbdbd1acb02379d33abeb108$" "PUT path")
    expect_match("${body}" "\"lufs\":-?[0-9]+\\.[0-9][0-9]" "PUT body lufs")
    expect_match("${body}" "\"peakDb\":-?[0-9]+\\.[0-9][0-9]" "PUT body peakDb")
    expect_match("${body}" "\"method\":\"(r128|rms)\"" "PUT body method")
  endif()
endforeach()
expect_eq(${saw_put} 1 "PUT seen in log")

# ---- 4. Desktop, no token: analysis runs, PUT skipped
reset_log()
run_loudness(r4 "${WORK}/t90-4.db" --config-dir "${empty_cfg}" "${SONGS}/techno.mod")
expect_match("${r4}" "loudness_source=analysis" "analysis without token")
expect_match("${r4}" "loudness_put=skipped_no_token" "PUT skipped without token")
loudness_log(l4)
expect_eq(${l4_put} 0 "no-token PUT requests")

# ---- 5. Endpoint missing (route absent): degrade to local only, say so
reset_log()
run_loudness(r5 "${WORK}/t90-5.db" --machine a1200 --config-dir "${cfg}" "${SONGS}/nogenre.mod")
expect_match("${r5}" "devilbox_missing=loudness" "missing endpoint")
expect_match("${r5}" "loudness_source=none" "missing endpoint leaves no gain")

# ---- 6. Second run with the same DB: stored gain, no request
reset_log()
execute_process(COMMAND "${CLI}" loudness --db "${WORK}/t90-1.db" --machine a1200 "${SONGS}/ambient.mod"
  OUTPUT_VARIABLE r6 RESULT_VARIABLE rc)
expect_match("${r6}" "loudness_source=stored gain_cdb=540" "stored gain")
loudness_log(l6)
expect_eq(${l6_get} 0 "stored GET requests")

message(STATUS "loudness_fetch_ok")
